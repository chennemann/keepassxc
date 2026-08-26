/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "AdditionalUrlQueryMatcher.h"

#include <QRegularExpression>
#include <QUrlQuery>
#include <QVector>

#include <algorithm>
#include <functional>

namespace
{
    using QueryItems = QList<QPair<QString, QString>>;

    QString extractQuery(const QString& pattern)
    {
        const auto queryStart = pattern.indexOf('?');
        const auto fragmentStart = pattern.indexOf('#');
        if (queryStart < 0 || (fragmentStart >= 0 && queryStart > fragmentStart)) {
            return {};
        }

        const auto queryEnd = fragmentStart < 0 ? pattern.size() : fragmentStart;
        return pattern.mid(queryStart + 1, queryEnd - queryStart - 1);
    }

    QueryItems getPatternItems(const QString& pattern)
    {
        return QUrlQuery(extractQuery(pattern)).queryItems(QUrl::FullyEncoded);
    }

    bool haveValidNames(const QueryItems& items)
    {
        return std::none_of(items.cbegin(), items.cend(), [](const auto& item) { return item.first.contains('*'); });
    }
} // namespace

bool Fork::AdditionalUrlQueryMatcher::isPatternValid(const QString& pattern)
{
    return haveValidNames(getPatternItems(pattern));
}

bool Fork::AdditionalUrlQueryMatcher::matches(const QString& pattern, const QUrl& visitedUrl)
{
    const auto patternItems = getPatternItems(pattern);
    if (patternItems.isEmpty()) {
        return true;
    }

    if (!haveValidNames(patternItems)) {
        return false;
    }

    const auto visitedItems = QUrlQuery(visitedUrl).queryItems(QUrl::FullyEncoded);
    QVector<QRegularExpression> valuePatterns;
    valuePatterns.reserve(patternItems.size());
    for (const auto& patternItem : patternItems) {
        auto valuePattern = QRegularExpression::escape(patternItem.second);
        valuePattern.replace(QRegularExpression::escape("*"), ".*");
        valuePatterns.append(QRegularExpression(QRegularExpression::anchoredPattern(valuePattern)));
    }

    // Find a one-to-one assignment so a broad constraint can give up an occurrence needed by a narrower one.
    QVector<int> assignedConstraints(visitedItems.size(), -1);
    std::function<bool(int, QVector<bool>&)> assignConstraint = [&](int constraintIndex, QVector<bool>& visited) {
        for (int itemIndex = 0; itemIndex < visitedItems.size(); ++itemIndex) {
            if (visited[itemIndex] || patternItems[constraintIndex].first != visitedItems[itemIndex].first
                || !valuePatterns[constraintIndex].match(visitedItems[itemIndex].second).hasMatch()) {
                continue;
            }

            visited[itemIndex] = true;
            if (assignedConstraints[itemIndex] == -1 || assignConstraint(assignedConstraints[itemIndex], visited)) {
                assignedConstraints[itemIndex] = constraintIndex;
                return true;
            }
        }

        return false;
    };

    for (int constraintIndex = 0; constraintIndex < patternItems.size(); ++constraintIndex) {
        QVector<bool> visited(visitedItems.size(), false);
        if (!assignConstraint(constraintIndex, visited)) {
            return false;
        }
    }

    return true;
}
