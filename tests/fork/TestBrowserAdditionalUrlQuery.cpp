/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "TestBrowser.h"

#include <QTest>

void TestBrowser::testAdditionalUrlQueryMatching_data()
{
    QTest::addColumn<QString>("pattern");
    QTest::addColumn<QString>("visitedUrl");
    QTest::addColumn<bool>("matches");

    QTest::newRow("required parameter after unrelated parameter")
        << "https://example.com/login?tenant=acme*"
        << "https://example.com/login?state=random&tenant=acme-prod" << true;
    QTest::newRow("required parameter first") << "https://example.com/login?tenant=acme*"
                                              << "https://example.com/login?tenant=acme-prod&state=random" << true;
    QTest::newRow("required parameter in middle")
        << "https://example.com/login?tenant=acme*"
        << "https://example.com/login?state=random&tenant=acme-prod&prompt=login" << true;
    QTest::newRow("multiple reordered parameters with unrelated parameters")
        << "https://example.com/login?tenant=acme*&mode=admin&client_id=123"
        << "https://example.com/login?state=random&client_id=123&tenant=acme-prod&mode=admin" << true;
    QTest::newRow("exact value") << "https://example.com/login?mode=admin"
                                 << "https://example.com/login?mode=admin" << true;
    QTest::newRow("incorrect exact value") << "https://example.com/login?mode=admin"
                                           << "https://example.com/login?mode=administrator" << false;
    QTest::newRow("prefix glob") << "https://example.com/login?tenant=acme*"
                                 << "https://example.com/login?tenant=acme-prod" << true;
    QTest::newRow("prefix glob is anchored") << "https://example.com/login?tenant=acme*"
                                             << "https://example.com/login?tenant=my-acme-prod" << false;
    QTest::newRow("suffix glob") << "https://example.com/login?tenant=*prod"
                                 << "https://example.com/login?tenant=acme-prod" << true;
    QTest::newRow("contains glob") << "https://example.com/login?tenant=*me-pr*"
                                   << "https://example.com/login?tenant=acme-prod" << true;
    QTest::newRow("regular expression characters are literal") << "https://example.com/login?code=a.b+*"
                                                               << "https://example.com/login?code=a.b+suffix" << true;
    QTest::newRow("regular expression syntax is not interpreted")
        << "https://example.com/login?code=a.b+*"
        << "https://example.com/login?code=axbbbb-suffix" << false;
    QTest::newRow("presence glob with empty value") << "https://example.com/login?prompt=*"
                                                    << "https://example.com/login?prompt=" << true;
    QTest::newRow("presence glob requires parameter") << "https://example.com/login?prompt=*"
                                                      << "https://example.com/login?mode=admin" << false;
    QTest::newRow("missing required parameter") << "https://example.com/login?tenant=acme*"
                                                << "https://example.com/login?state=random" << false;
    QTest::newRow("all constraints required") << "https://example.com/login?tenant=acme*&mode=admin"
                                              << "https://example.com/login?tenant=acme-prod" << false;
    QTest::newRow("duplicate visited parameter can satisfy constraint")
        << "https://example.com/login?scope=profile"
        << "https://example.com/login?scope=openid&scope=profile" << true;
    QTest::newRow("repeated constraints use distinct occurrences")
        << "https://example.com/login?scope=openid&scope=profile"
        << "https://example.com/login?scope=profile&scope=openid" << true;
    QTest::newRow("repeated constraints cannot reuse occurrence")
        << "https://example.com/login?scope=openid&scope=openid"
        << "https://example.com/login?scope=openid" << false;
    QTest::newRow("overlapping constraints can reassign occurrences")
        << "https://example.com/login?scope=*&scope=openid"
        << "https://example.com/login?scope=openid&scope=profile" << true;
    QTest::newRow("parameter names are case sensitive") << "https://example.com/login?Tenant=acme"
                                                        << "https://example.com/login?tenant=acme" << false;
    QTest::newRow("parameter values are case sensitive") << "https://example.com/login?tenant=Acme"
                                                         << "https://example.com/login?tenant=acme" << false;
    QTest::newRow("encoded delimiters and characters remain values")
        << "https://example.com/login?amp=a%26b&equals=a%3Db&slash=%2Fcallback&space=hello%20world"
        << "https://example.com/login?space=hello%20world&slash=%2Fcallback&equals=a%3Db&amp=a%26b" << true;
    QTest::newRow("percent escape hex case is normalized") << "https://example.com/login?slash=%2fcallback"
                                                           << "https://example.com/login?slash=%2Fcallback" << true;
    QTest::newRow("encoded slash remains distinct from literal slash")
        << "https://example.com/login?slash=%2Fcallback"
        << "https://example.com/login?slash=/callback" << false;
    QTest::newRow("encoded space remains distinct from plus") << "https://example.com/login?space=hello%20world"
                                                              << "https://example.com/login?space=hello+world" << false;
    QTest::newRow("encoded ampersand is not a separator") << "https://example.com/login?payload=a%26b"
                                                          << "https://example.com/login?payload=a&b=" << false;
    QTest::newRow("encoded equals is not a separator") << "https://example.com/login?payload=a%3Db"
                                                       << "https://example.com/login?payload=a&b=" << false;
    QTest::newRow("oauth nested redirect URI")
        << "https://accounts.example.com/oauth?client_id=123&redirect_uri=*login.target.example*"
        << "https://accounts.example.com/"
           "oauth?state=random&redirect_uri=https%3A%2F%2Flogin.target.example%2Fcallback&client_id=123"
        << true;
    QTest::newRow("oauth nested redirect URI rejects other host")
        << "https://accounts.example.com/oauth?client_id=123&redirect_uri=*login.target.example*"
        << "https://accounts.example.com/"
           "oauth?state=random&redirect_uri=https%3A%2F%2Fevil.example%2Fcallback&client_id=123"
        << false;
    QTest::newRow("pattern without query ignores visited query")
        << "https://example.com/login*"
        << "https://example.com/login?tenant=other&state=random" << true;
    QTest::newRow("quoted URL preserves query order") << "\"https://example.com/login?tenant=acme&mode=admin\""
                                                      << "https://example.com/login?tenant=acme&mode=admin" << true;
    QTest::newRow("quoted URL rejects reordered query") << "\"https://example.com/login?tenant=acme&mode=admin\""
                                                        << "https://example.com/login?mode=admin&tenant=acme" << false;
    QTest::newRow("quoted URL preserves query encoding") << "\"https://example.com/login?redirect=%2Fcallback\""
                                                         << "https://example.com/login?redirect=%2fcallback" << false;
    QTest::newRow("fragments do not participate") << "https://example.com/login?tenant=*#pattern-fragment"
                                                  << "https://example.com/login?tenant=acme#visited-fragment" << true;
    QTest::newRow("wildcard parameter name is rejected") << "https://example.com/login?ten*=acme"
                                                         << "https://example.com/login?tenant=acme" << false;
}

void TestBrowser::testAdditionalUrlQueryMatching()
{
    QFETCH(QString, pattern);
    QFETCH(QString, visitedUrl);
    QFETCH(bool, matches);

    QCOMPARE(m_browserService->handleURL(pattern, visitedUrl, visitedUrl, false, true), matches);
}

void TestBrowser::testMainEntryQueryMatchingUnchanged()
{
    const QString entryUrl = "https://example.com/login?tenant=acme";
    const QString visitedUrl = "https://example.com/login?tenant=other";

    QVERIFY(m_browserService->handleURL(entryUrl, visitedUrl, visitedUrl));
}
