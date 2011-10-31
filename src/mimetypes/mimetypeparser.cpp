/**************************************************************************
**
** This file is part of QMime
**
** Based on Qt Creator source code
**
** Qt Creator Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
**************************************************************************/

#include "mimetypeparser_p.h"

#include "qmimetype_p.h"
#include "qmimemagicrulematcher_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QPair>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>
#include <QtCore/QStack>

QT_BEGIN_NAMESPACE

// XML tags in MIME files
const char *const mimeInfoTagC = "mime-info";
const char *const mimeTypeTagC = "mime-type";
const char *const mimeTypeAttributeC = "type";
const char *const subClassTagC = "sub-class-of";
const char *const commentTagC = "comment";
const char *const genericIconTagC = "generic-icon";
const char *const iconTagC = "icon";
const char *const nameAttributeC = "name";
const char *const globTagC = "glob";
const char *const aliasTagC = "alias";
const char *const patternAttributeC = "pattern";
const char *const weightAttributeC = "weight";
const char *const caseSensitiveAttributeC = "case-sensitive";
const char *const localeAttributeC = "xml:lang";

const char *const magicTagC = "magic";
const char *const priorityAttributeC = "priority";

const char *const matchTagC = "match";
const char *const matchValueAttributeC = "value";
const char *const matchTypeAttributeC = "type";
const char *const matchOffsetAttributeC = "offset";
const char *const matchMaskAttributeC = "mask";

/*!
    \class MimeTypeParser
    \brief MIME type parser that builds a MIME database hierarchy by adding to QMimeDatabasePrivate

    Populates MimeDataBase

    \sa QMimeDatabase, QMimeMagicRuleMatcher, MagicRule, MagicStringRule, MagicByteRule, GlobPattern
    \sa MimeTypeParser
*/


/*!
    \class BaseMimeTypeParser
    \brief Generic parser for a sequence of <mime-type>.

    Calls abstract handler function process for QMimeType it finds.

    \sa QMimeDatabase, QMimeMagicRuleMatcher, MagicRule, MagicStringRule, MagicByteRule, GlobPattern
    \sa MimeTypeParser
*/

/*!
    \fn virtual bool BaseMimeTypeParser::process(const QMimeType &t, QString *errorMessage) = 0;
    \brief Overwrite to process the sequence of parsed data
*/

BaseMimeTypeParser::ParseState BaseMimeTypeParser::nextState(ParseState currentState, const QStringRef &startElement)
{
    switch (currentState) {
    case ParseBeginning:
        if (startElement == QLatin1String(mimeInfoTagC))
            return ParseMimeInfo;
        if (startElement == QLatin1String(mimeTypeTagC))
            return ParseMimeType;
        return ParseError;
    case ParseMimeInfo:
        return startElement == QLatin1String(mimeTypeTagC) ? ParseMimeType : ParseError;
    case ParseMimeType:
    case ParseComment:
    case ParseGenericIcon:
    case ParseIcon:
    case ParseGlobPattern:
    case ParseSubClass:
    case ParseAlias:
    case ParseOtherMimeTypeSubTag:
    case ParseMagicMatchRule:
        if (startElement == QLatin1String(mimeTypeTagC)) // Sequence of <mime-type>
            return ParseMimeType;
        if (startElement == QLatin1String(commentTagC ))
            return ParseComment;
        if (startElement == QLatin1String(genericIconTagC))
            return ParseGenericIcon;
        if (startElement == QLatin1String(iconTagC))
            return ParseIcon;
        if (startElement == QLatin1String(globTagC))
            return ParseGlobPattern;
        if (startElement == QLatin1String(subClassTagC))
            return ParseSubClass;
        if (startElement == QLatin1String(aliasTagC))
            return ParseAlias;
        if (startElement == QLatin1String(magicTagC))
            return ParseMagic;
        if (startElement == QLatin1String(matchTagC))
            return ParseMagicMatchRule;
        return ParseOtherMimeTypeSubTag;
    case ParseMagic:
        if (startElement == QLatin1String(matchTagC))
            return ParseMagicMatchRule;
        break;
    case ParseError:
        break;
    }
    return ParseError;
}

// Parse int number from an (attribute) string)
static bool parseNumber(const QString &n, int *target, QString *errorMessage)
{
    bool ok;
    *target = n.toInt(&ok);
    if (!ok) {
        *errorMessage = QString::fromLatin1("Not a number '%1'.").arg(n);
        return false;
    }
    return true;
}

// Evaluate a magic match rule like
//  <match value="must be converted with BinHex" type="string" offset="11"/>
//  <match value="0x9501" type="big16" offset="0:64"/>
static bool createMagicMatchRule(const QXmlStreamAttributes &atts,
                                 QString *errorMessage, QMimeMagicRule *&rule)
{
    const QString type = atts.value(QLatin1String(matchTypeAttributeC)).toString();
    QMimeMagicRule::Type magicType = QMimeMagicRule::type(type.toLatin1());
    if (magicType == QMimeMagicRule::Invalid) {
        qWarning("%s: match type %s is not supported.", Q_FUNC_INFO, type.toUtf8().constData());
        return true;
    }
    const QString value = atts.value(QLatin1String(matchValueAttributeC)).toString();
    if (value.isEmpty()) {
        *errorMessage = QString::fromLatin1("Empty match value detected.");
        return false;
    }
    // Parse for offset as "1" or "1:10"
    int startPos, endPos;
    const QString offsetS = atts.value(QLatin1String(matchOffsetAttributeC)).toString();
    const int colonIndex = offsetS.indexOf(QLatin1Char(':'));
    const QString startPosS = colonIndex == -1 ? offsetS : offsetS.mid(0, colonIndex);
    const QString endPosS   = colonIndex == -1 ? offsetS : offsetS.mid(colonIndex + 1);
    if (!parseNumber(startPosS, &startPos, errorMessage) || !parseNumber(endPosS, &endPos, errorMessage))
        return false;
    const QString mask = atts.value(QLatin1String(matchMaskAttributeC)).toString();

    rule = new QMimeMagicRule(magicType, value.toUtf8(), startPos, endPos, mask.toLatin1());

    return true;
}

bool BaseMimeTypeParser::parse(QIODevice *dev, const QString &fileName, QString *errorMessage)
{
    QMimeTypePrivate data;
    int priority = 50;
    QStack<QMimeMagicRule *> currentRules; // stack for the nesting of rules
    QList<QMimeMagicRule> rules; // toplevel rules
    QXmlStreamReader reader(dev);
    ParseState ps = ParseBeginning;
    QXmlStreamAttributes atts;
    while (!reader.atEnd()) {
        switch (reader.readNext()) {
        case QXmlStreamReader::StartElement:
            ps = nextState(ps, reader.name());
            atts = reader.attributes();
            switch (ps) {
            case ParseMimeType: { // start parsing a MIME type name
                const QString name = atts.value(QLatin1String(mimeTypeAttributeC)).toString();
                if (name.isEmpty()) {
                    reader.raiseError(QString::fromLatin1("Missing '%1'-attribute").arg(QString::fromLatin1(mimeTypeAttributeC)));
                } else {
                    data.name = name;
                }
            }
                break;
            case ParseGenericIcon:
                data.genericIconName = atts.value(QLatin1String(nameAttributeC)).toString();
                break;
            case ParseIcon:
                data.iconName = atts.value(QLatin1String(nameAttributeC)).toString();
                break;
            case ParseGlobPattern: {
                const QString pattern = atts.value(QLatin1String(patternAttributeC)).toString();
                unsigned weight = atts.value(QLatin1String(weightAttributeC)).toString().toInt();
                const bool caseSensitive = atts.value(QLatin1String(caseSensitiveAttributeC)).toString() == QLatin1String("true");

                if (weight == 0)
                    weight = QMimeGlobPattern::DefaultWeight;

                Q_ASSERT(!data.name.isEmpty());
                const QMimeGlobPattern glob(pattern, data.name, weight, caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
                if (!process(glob, errorMessage))   // for actual glob matching
                    return false;
                data.addGlobPattern(pattern); // just for QMimeType::globPatterns()
            }
                break;
            case ParseSubClass: {
                const QString inheritsFrom = atts.value(QLatin1String(mimeTypeAttributeC)).toString();
                if (!inheritsFrom.isEmpty())
                    processParent(data.name, inheritsFrom);
            }
                break;
            case ParseComment: {
                // comments have locale attributes. We want the default, English one
                QString locale = atts.value(QLatin1String(localeAttributeC)).toString();
                const QString comment = reader.readElementText();
                if (locale.isEmpty())
                    data.comment = comment;
                else
                    data.localeComments.insert(locale, comment);
            }
                break;
            case ParseAlias: {
                const QString alias = atts.value(QLatin1String(mimeTypeAttributeC)).toString();
                if (!alias.isEmpty())
                    processAlias(alias, data.name);
            }
                break;
            case ParseMagic: {
                priority = 50;
                const QString priorityS = atts.value(QLatin1String(priorityAttributeC)).toString();
                if (!priorityS.isEmpty()) {
                    if (!parseNumber(priorityS, &priority, errorMessage))
                        return false;

                }
                currentRules.clear();
                //qDebug() << "MAGIC start for mimetype" << data.name;
            }
                break;
            case ParseMagicMatchRule: {
                QMimeMagicRule *rule = 0;
                if (!createMagicMatchRule(atts, errorMessage, rule))
                    return false;
                QList<QMimeMagicRule> *ruleList;
                if (currentRules.isEmpty())
                    ruleList = &rules;
                else // nest this rule into the proper parent
                    ruleList = &currentRules.top()->m_subMatches;
                ruleList->append(*rule);
                //qDebug() << " MATCH added. Stack size was" << currentRules.size();
                currentRules.push(&ruleList->last());
                delete rule;
                break;
            }
            case ParseError:
                reader.raiseError(QString::fromLatin1("Unexpected element <%1>").
                                  arg(reader.name().toString()));
                break;
            default:
                break;
            }
            break;
        // continue switch QXmlStreamReader::Token...
        case QXmlStreamReader::EndElement: // Finished element
        {
            const QStringRef elementName = reader.name();
            if (elementName == QLatin1String(mimeTypeTagC)) {
                if (!process(QMimeType(data), errorMessage))
                    return false;
                data.clear();
            } else if (elementName == QLatin1String(matchTagC)) {
                // Closing a <match> tag, pop stack
                currentRules.pop();
                //qDebug() << " MATCH closed. Stack size is now" << currentRules.size();
            } else if (elementName == QLatin1String(magicTagC)) {
                //qDebug() << "MAGIC ended, we got" << rules.count() << "rules, with prio" << priority;
                // Finished a <magic> sequence
                QMimeMagicRuleMatcher ruleMatcher(data.name, priority);
                ruleMatcher.addRules(rules);
                processMagicMatcher(ruleMatcher);
                rules.clear();
            }
            break;
        }
        default:
            break;
        }
    }

    if (reader.hasError()) {
        if (errorMessage)
            *errorMessage = QString::fromLatin1("An error has been encountered at line %1 of %2: %3:").arg(reader.lineNumber()).arg(fileName, reader.errorString());
        return false;
    }

    return true;
}

QT_END_NAMESPACE
