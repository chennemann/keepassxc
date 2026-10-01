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

#include "core/Entry.h"
#include "core/Group.h"
#include "core/Totp.h"
#include "crypto/Crypto.h"
#include "gui/entry/EntryModel.h"
#include "gui/entry/EntryView.h"
#include "mock/MockClock.h"

#include <QHeaderView>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTest>

class TestCurrentTotp : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        Config::createTempFileInstance();
        QVERIFY(Crypto::init());
    }
    void cleanup()
    {
        MockClock::teardown();
    }
    void tokens_data();
    void tokens();
    void emptyAndInvalid();
    void refreshAndLayout();
};

void TestCurrentTotp::tokens_data()
{
    QTest::addColumn<uint>("digits");
    QTest::addColumn<uint>("period");
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("after");
    QTest::newRow("six digits") << 6u << 30u << QString("287082") << QString("359152");
    QTest::newRow("eight digits") << 8u << 30u << QString("94287082") << QString("37359152");
    QTest::newRow("custom period") << 6u << 60u << QString("755224") << QString("287082");
}

void TestCurrentTotp::tokens()
{
    QFETCH(uint, digits);
    QFETCH(uint, period);
    QFETCH(QString, before);
    QFETCH(QString, after);
    auto clock = new MockClock(QDateTime::fromSecsSinceEpoch(59, Qt::UTC));
    MockClock::setup(clock);
    Group group;
    auto entry = new Entry();
    entry->setGroup(&group);
    entry->setTotp(Totp::createSettings("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ", digits, period));
    EntryModel model;
    model.setGroup(&group);
    auto index = model.index(0, EntryModel::CurrentTotp);
    QCOMPARE(index.data().toString(), before);
    QCOMPARE(index.data(Qt::UserRole).toString(), before);
    clock->advanceSecond(1);
    QCOMPARE(index.data().toString(), after);
    // Search results use the same column and the same current value.
    model.setEntries({entry});
    QCOMPARE(model.index(0, EntryModel::CurrentTotp).data().toString(), after);
    QVERIFY(model.index(0, EntryModel::Totp).data(Qt::UserRole).toBool());
}

void TestCurrentTotp::emptyAndInvalid()
{
    Group group;
    auto entry = new Entry();
    entry->setGroup(&group);
    EntryModel model;
    model.setGroup(&group);
    auto index = model.index(0, EntryModel::CurrentTotp);
    QVERIFY(index.data().toString().isEmpty());
    entry->setTotp(Totp::createSettings("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ"));
    entry->totpSettings()->step = 0;
    QVERIFY(!entry->hasValidTotp());
    QVERIFY(index.data().toString().isEmpty());
    entry->setTotp(Totp::createSettings("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ"));
    QVERIFY(!index.data().toString().isEmpty());
    entry->setTotp({});
    QVERIFY(index.data().toString().isEmpty());
}

void TestCurrentTotp::refreshAndLayout()
{
    auto clock = new MockClock(QDateTime::fromSecsSinceEpoch(59, Qt::UTC));
    MockClock::setup(clock);
    Group group;
    auto entry = new Entry();
    entry->setGroup(&group);
    entry->setTotp(Totp::createSettings("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ"));
    EntryView view;
    view.displayGroup(&group);
    QVERIFY(view.header()->isSectionHidden(EntryModel::CurrentTotp));

    QStandardItemModel oldModel(0, EntryModel::CurrentTotp);
    QHeaderView oldHeader(Qt::Horizontal);
    oldHeader.setModel(&oldModel);
    QVERIFY(view.setViewState(oldHeader.saveState()));
    QVERIFY(view.header()->isSectionHidden(EntryModel::CurrentTotp));

    view.header()->showSection(EntryModel::CurrentTotp);
    auto state = view.viewState();
    view.header()->hideSection(EntryModel::CurrentTotp);
    QVERIFY(view.setViewState(state));
    QVERIFY(!view.header()->isSectionHidden(EntryModel::CurrentTotp));
    view.show();
    auto model = view.findChild<EntryModel*>();
    QVERIFY(model);
    QSignalSpy changed(model, &QAbstractItemModel::dataChanged);
    clock->advanceSecond(1);
    QTRY_VERIFY(!changed.isEmpty());
    QCOMPARE(changed.last().at(0).value<QModelIndex>().column(), int(EntryModel::CurrentTotp));
    QCOMPARE(model->index(0, EntryModel::CurrentTotp).data().toString(), QString("359152"));
    view.header()->hideSection(EntryModel::CurrentTotp);
    changed.clear();
    QTest::qWait(1100);
    QVERIFY(changed.isEmpty());
    view.displaySearch({});
    view.header()->showSection(EntryModel::CurrentTotp);
    QTest::qWait(1100);
    QVERIFY(changed.isEmpty());
}

QTEST_MAIN(TestCurrentTotp)
#include "TestCurrentTotp.moc"
