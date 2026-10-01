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

#include "CurrentTotpColumn.h"

#include "core/Entry.h"
#include "gui/entry/EntryModel.h"

#include <QHeaderView>
#include <QTimer>
#include <QTreeView>

QString Fork::CurrentTotpColumn::token(const Entry* entry)
{
    bool valid = false;
    const auto value = entry->totp(&valid);
    return valid ? value : QString();
}

void Fork::CurrentTotpColumn::installRefresh(EntryModel* model, QTreeView* view)
{
    auto timer = new QTimer(view);
    timer->setInterval(1000);
    QObject::connect(timer, &QTimer::timeout, model, [model, view] {
        if (!view->isVisible() || view->header()->isSectionHidden(EntryModel::CurrentTotp) || model->rowCount() == 0) {
            return;
        }

        // Read tokens on demand, using each entry's own period and encoder. Never cache codes.
        emit model->dataChanged(model->index(0, EntryModel::CurrentTotp),
                                model->index(model->rowCount() - 1, EntryModel::CurrentTotp),
                                {Qt::DisplayRole, Qt::UserRole});
    });
    timer->start();
}

bool Fork::CurrentTotpColumn::restoreState(QHeaderView* header, const QByteArray& state)
{
    const bool restored = header->restoreState(state);
    QHeaderView savedHeader(Qt::Horizontal);
    // Finish the empty header's deferred initialization before restoring its sections.
    savedHeader.count();
    // Old layouts have no visibility preference for the newly appended column.
    if (restored && savedHeader.restoreState(state) && savedHeader.count() <= EntryModel::CurrentTotp) {
        header->hideSection(EntryModel::CurrentTotp);
    }
    return restored;
}
