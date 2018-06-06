// Copyright (c) 2017-2018 The Popchain Core Developers
#include "netbase.h"
#include "test/test_pop.h"

#include <string>

#include <boost/assign/list_of.hpp>
#include <boost/test/unit_test.hpp>
#include <vector>

using namespace std;
BOOST_FIXTURE_TEST_SUITE(netbase_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(netbase_networks)
{
    BOOST_CHECK(CNetAddr("127.0.0.1").GetNetwork()                              == NET_IPV4);
    BOOST_CHECK(CNetAddr("::1").GetNetwork()                                    == NET_IPV6);
    BOOST_CHECK(CNetAddr("8.8.8.8").GetNetwork()                                == NET_IPV4);
    BOOST_CHECK(CNetAddr("2001::8888").GetNetwork()                             == NET_IPV6);
    BOOST_CHECK(CNetAddr("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").GetNetwork() == NET_TOR);
}

BOOST_AUTO_TEST_CASE(netbase_properties)
{
    BOOST_CHECK(CNetAddr("127.0.0.1").IsIPv4());
    BOOST_CHECK(CNetAddr("::FFFF:192.168.1.1").IsIPv4());
    BOOST_CHECK(CNetAddr("::1").IsIPv6());
    BOOST_CHECK(CNetAddr("10.0.0.1").IsRFC1918());
    BOOST_CHECK(CNetAddr("192.168.1.1").IsRFC1918());
    BOOST_CHECK(CNetAddr("172.31.255.255").IsRFC1918());
    BOOST_CHECK(CNetAddr("2001:0DB8::").IsRFC3849());
    BOOST_CHECK(CNetAddr("169.254.1.1").IsRFC3927());
    BOOST_CHECK(CNetAddr("2002::1").IsRFC3964());
    BOOST_CHECK(CNetAddr("FC00::").IsRFC4193());
    BOOST_CHECK(CNetAddr("2001::2").IsRFC4380());
    BOOST_CHECK(CNetAddr("2001:10::").IsRFC4843());
    BOOST_CHECK(CNetAddr("FE80::").IsRFC4862());
    BOOST_CHECK(CNetAddr("64:FF9B::").IsRFC6052());
    BOOST_CHECK(CNetAddr("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").IsTor());
    BOOST_CHECK(CNetAddr("127.0.0.1").IsLocal());
    BOOST_CHECK(CNetAddr("::1").IsLocal());
    BOOST_CHECK(CNetAddr("8.8.8.8").IsRoutable());
    BOOST_CHECK(CNetAddr("2001::1").IsRoutable());
    BOOST_CHECK(CNetAddr("127.0.0.1").IsValid());
}

bool static TestSplitHost(string test, string host, int port)
{
    string hostOut;
    int portOut = -1;
    SplitHostPort(test, portOut, hostOut);
    return hostOut == host && port == portOut;
}

BOOST_AUTO_TEST_CASE(netbase_splithost)
{
    BOOST_CHECK(TestSplitHost("www.bitcoin.org", "www.bitcoin.org", -1));
    BOOST_CHECK(TestSplitHost("[www.bitcoin.org]", "www.bitcoin.org", -1));
    BOOST_CHECK(TestSplitHost("www.bitcoin.org:80", "www.bitcoin.org", 80));
    BOOST_CHECK(TestSplitHost("[www.bitcoin.org]:80", "www.bitcoin.org", 80));
    BOOST_CHECK(TestSplitHost("127.0.0.1", "127.0.0.1", -1));
    BOOST_CHECK(TestSplitHost("127.0.0.1:9888", "127.0.0.1", 9888));
    BOOST_CHECK(TestSplitHost("[127.0.0.1]", "127.0.0.1", -1));
    BOOST_CHECK(TestSplitHost("[127.0.0.1]:9888", "127.0.0.1", 9888));
    BOOST_CHECK(TestSplitHost("::ffff:127.0.0.1", "::ffff:127.0.0.1", -1));
    BOOST_CHECK(TestSplitHost("[::ffff:127.0.0.1]:9888", "::ffff:127.0.0.1", 9888));
    BOOST_CHECK(TestSplitHost("[::]:9888", "::", 9888));
    BOOST_CHECK(TestSplitHost("::9888", "::9888", -1));
    BOOST_CHECK(TestSplitHost(":9888", "", 9888));
    BOOST_CHECK(TestSplitHost("[]:9888", "", 9888));
    BOOST_CHECK(TestSplitHost("", "", -1));
}

bool static TestParse(string src, string canon)
{
    CService addr;
    if (!LookupNumeric(src.c_str(), addr, 65535))
        return canon == "";
    return canon == addr.ToString();
}

BOOST_AUTO_TEST_CASE(netbase_lookupnumeric)
{
    BOOST_CHECK(TestParse("127.0.0.1", "127.0.0.1:65535"));
    BOOST_CHECK(TestParse("127.0.0.1:9888", "127.0.0.1:9888"));
    BOOST_CHECK(TestParse("::ffff:127.0.0.1", "127.0.0.1:65535"));
    BOOST_CHECK(TestParse("::", "[::]:65535"));
    BOOST_CHECK(TestParse("[::]:9888", "[::]:9888"));
    BOOST_CHECK(TestParse("[127.0.0.1]", "127.0.0.1:65535"));
    BOOST_CHECK(TestParse(":::", ""));
}

BOOST_AUTO_TEST_CASE(onioncat_test)
{
    // values from https://web.archive.org/web/20121122003543/http://www.cypherpunk.at/onioncat/wiki/OnionCat
    CNetAddr addr1("5wyqrzbvrdsumnok.onion");
    CNetAddr addr2("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca");
    BOOST_CHECK(addr1 == addr2);
    BOOST_CHECK(addr1.IsTor());
    BOOST_CHECK(addr1.ToStringIP() == "5wyqrzbvrdsumnok.onion");
    BOOST_CHECK(addr1.IsRoutable());
}

BOOST_AUTO_TEST_CASE(supop_test)
{
    BOOST_CHECK(CSubNet("1.2.3.0/24") == CSubNet("1.2.3.0/255.255.255.0"));
    BOOST_CHECK(CSubNet("1.2.3.0/24") != CSubNet("1.2.4.0/255.255.255.0"));
    BOOST_CHECK(CSubNet("1.2.3.0/24").Match(CNetAddr("1.2.3.4")));
    BOOST_CHECK(!CSubNet("1.2.2.0/24").Match(CNetAddr("1.2.3.4")));
    BOOST_CHECK(CSubNet("1.2.3.4").Match(CNetAddr("1.2.3.4")));
    BOOST_CHECK(CSubNet("1.2.3.4/32").Match(CNetAddr("1.2.3.4")));
    BOOST_CHECK(!CSubNet("1.2.3.4").Match(CNetAddr("5.6.7.8")));
    BOOST_CHECK(!CSubNet("1.2.3.4/32").Match(CNetAddr("5.6.7.8")));
    BOOST_CHECK(CSubNet("::ffff:127.0.0.1").Match(CNetAddr("127.0.0.1")));
    BOOST_CHECK(CSubNet("1:2:3:4:5:6:7:8").Match(CNetAddr("1:2:3:4:5:6:7:8")));
    BOOST_CHECK(!CSubNet("1:2:3:4:5:6:7:8").Match(CNetAddr("1:2:3:4:5:6:7:9")));
    BOOST_CHECK(CSubNet("1:2:3:4:5:6:7:0/112").Match(CNetAddr("1:2:3:4:5:6:7:1234")));
    BOOST_CHECK(CSubNet("192.168.0.1/24").Match(CNetAddr("192.168.0.2")));
    BOOST_CHECK(CSubNet("192.168.0.20/29").Match(CNetAddr("192.168.0.18")));
    BOOST_CHECK(CSubNet("1.2.2.1/24").Match(CNetAddr("1.2.2.4")));
    BOOST_CHECK(CSubNet("1.2.2.110/31").Match(CNetAddr("1.2.2.111")));
    BOOST_CHECK(CSubNet("1.2.2.20/26").Match(CNetAddr("1.2.2.63")));
    // All-Matching IPv6 Matches arbitrary IPv4 and IPv6
    BOOST_CHECK(CSubNet("::/0").Match(CNetAddr("1:2:3:4:5:6:7:1234")));
    BOOST_CHECK(CSubNet("::/0").Match(CNetAddr("1.2.3.4")));
    // All-Matching IPv4 does not Match IPv6
    BOOST_CHECK(!CSubNet("0.0.0.0/0").Match(CNetAddr("1:2:3:4:5:6:7:1234")));
    // Invalid supops Match nothing (not even invalid addresses)
    BOOST_CHECK(!CSubNet().Match(CNetAddr("1.2.3.4")));
    BOOST_CHECK(!CSubNet("").Match(CNetAddr("4.5.6.7")));
    BOOST_CHECK(!CSubNet("bloop").Match(CNetAddr("0.0.0.0")));
    BOOST_CHECK(!CSubNet("bloop").Match(CNetAddr("hab")));
    // Check valid/invalid
    BOOST_CHECK(CSubNet("1.2.3.0/0").IsValid());
    BOOST_CHECK(!CSubNet("1.2.3.0/-1").IsValid());
    BOOST_CHECK(CSubNet("1.2.3.0/32").IsValid());
    BOOST_CHECK(!CSubNet("1.2.3.0/33").IsValid());
    BOOST_CHECK(CSubNet("1:2:3:4:5:6:7:8/0").IsValid());
    BOOST_CHECK(CSubNet("1:2:3:4:5:6:7:8/33").IsValid());
    BOOST_CHECK(!CSubNet("1:2:3:4:5:6:7:8/-1").IsValid());
    BOOST_CHECK(CSubNet("1:2:3:4:5:6:7:8/128").IsValid());
    BOOST_CHECK(!CSubNet("1:2:3:4:5:6:7:8/129").IsValid());
    BOOST_CHECK(!CSubNet("fuzzy").IsValid());

    //CNetAddr constructor test
    BOOST_CHECK(CSubNet(CNetAddr("127.0.0.1")).IsValid());
    BOOST_CHECK(CSubNet(CNetAddr("127.0.0.1")).Match(CNetAddr("127.0.0.1")));
    BOOST_CHECK(!CSubNet(CNetAddr("127.0.0.1")).Match(CNetAddr("127.0.0.2")));
    BOOST_CHECK(CSubNet(CNetAddr("127.0.0.1")).ToString() == "127.0.0.1/32");

    BOOST_CHECK(CSubNet(CNetAddr("1:2:3:4:5:6:7:8")).IsValid());
    BOOST_CHECK(CSubNet(CNetAddr("1:2:3:4:5:6:7:8")).Match(CNetAddr("1:2:3:4:5:6:7:8")));
    BOOST_CHECK(!CSubNet(CNetAddr("1:2:3:4:5:6:7:8")).Match(CNetAddr("1:2:3:4:5:6:7:9")));
    BOOST_CHECK(CSubNet(CNetAddr("1:2:3:4:5:6:7:8")).ToString() == "1:2:3:4:5:6:7:8/128");

    CSubNet supop = CSubNet("1.2.3.4/255.255.255.255");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.3.4/32");
    supop = CSubNet("1.2.3.4/255.255.255.254");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.3.4/31");
    supop = CSubNet("1.2.3.4/255.255.255.252");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.3.4/30");
    supop = CSubNet("1.2.3.4/255.255.255.248");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.3.0/29");
    supop = CSubNet("1.2.3.4/255.255.255.240");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.3.0/28");
    supop = CSubNet("1.2.3.4/255.255.255.224");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.3.0/27");
    supop = CSubNet("1.2.3.4/255.255.255.192");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.3.0/26");
    supop = CSubNet("1.2.3.4/255.255.255.128");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.3.0/25");
    supop = CSubNet("1.2.3.4/255.255.255.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.3.0/24");
    supop = CSubNet("1.2.3.4/255.255.254.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.2.0/23");
    supop = CSubNet("1.2.3.4/255.255.252.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.0.0/22");
    supop = CSubNet("1.2.3.4/255.255.248.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.0.0/21");
    supop = CSubNet("1.2.3.4/255.255.240.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.0.0/20");
    supop = CSubNet("1.2.3.4/255.255.224.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.0.0/19");
    supop = CSubNet("1.2.3.4/255.255.192.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.0.0/18");
    supop = CSubNet("1.2.3.4/255.255.128.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.0.0/17");
    supop = CSubNet("1.2.3.4/255.255.0.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.0.0/16");
    supop = CSubNet("1.2.3.4/255.254.0.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.0.0/15");
    supop = CSubNet("1.2.3.4/255.252.0.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.0.0.0/14");
    supop = CSubNet("1.2.3.4/255.248.0.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.0.0.0/13");
    supop = CSubNet("1.2.3.4/255.240.0.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.0.0.0/12");
    supop = CSubNet("1.2.3.4/255.224.0.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.0.0.0/11");
    supop = CSubNet("1.2.3.4/255.192.0.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.0.0.0/10");
    supop = CSubNet("1.2.3.4/255.128.0.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.0.0.0/9");
    supop = CSubNet("1.2.3.4/255.0.0.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.0.0.0/8");
    supop = CSubNet("1.2.3.4/254.0.0.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "0.0.0.0/7");
    supop = CSubNet("1.2.3.4/252.0.0.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "0.0.0.0/6");
    supop = CSubNet("1.2.3.4/248.0.0.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "0.0.0.0/5");
    supop = CSubNet("1.2.3.4/240.0.0.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "0.0.0.0/4");
    supop = CSubNet("1.2.3.4/224.0.0.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "0.0.0.0/3");
    supop = CSubNet("1.2.3.4/192.0.0.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "0.0.0.0/2");
    supop = CSubNet("1.2.3.4/128.0.0.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "0.0.0.0/1");
    supop = CSubNet("1.2.3.4/0.0.0.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "0.0.0.0/0");

    supop = CSubNet("1:2:3:4:5:6:7:8/ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
    BOOST_CHECK_EQUAL(supop.ToString(), "1:2:3:4:5:6:7:8/128");
    supop = CSubNet("1:2:3:4:5:6:7:8/ffff:0000:0000:0000:0000:0000:0000:0000");
    BOOST_CHECK_EQUAL(supop.ToString(), "1::/16");
    supop = CSubNet("1:2:3:4:5:6:7:8/0000:0000:0000:0000:0000:0000:0000:0000");
    BOOST_CHECK_EQUAL(supop.ToString(), "::/0");
    supop = CSubNet("1.2.3.4/255.255.232.0");
    BOOST_CHECK_EQUAL(supop.ToString(), "1.2.0.0/255.255.232.0");
    supop = CSubNet("1:2:3:4:5:6:7:8/ffff:ffff:ffff:fffe:ffff:ffff:ffff:ff0f");
    BOOST_CHECK_EQUAL(supop.ToString(), "1:2:3:4:5:6:7:8/ffff:ffff:ffff:fffe:ffff:ffff:ffff:ff0f");
}

BOOST_AUTO_TEST_CASE(netbase_getgroup)
{
    BOOST_CHECK(CNetAddr("127.0.0.1").GetGroup() == boost::assign::list_of(0x01)); // Local -> !Routable()
    BOOST_CHECK(CNetAddr("257.0.0.1").GetGroup() == boost::assign::list_of(0x02)(0x00)(0x00)(0x00)(0x00)); // !Valid -> !Routable()
    BOOST_CHECK(CNetAddr("10.0.0.1").GetGroup() == boost::assign::list_of(0x01)(0x0a)(0x00)); // RFC1918 -> !Routable()
    BOOST_CHECK(CNetAddr("169.254.1.1").GetGroup() == boost::assign::list_of(0x01)(0xa9)(0xfe)); // RFC3927 -> !Routable()
    BOOST_CHECK(CNetAddr("1.2.3.4").GetGroup() == boost::assign::list_of((unsigned char)NET_IPV4)(1)(2)); // IPv4
    BOOST_CHECK(CNetAddr("::FFFF:0:102:304").GetGroup() == boost::assign::list_of((unsigned char)NET_IPV4)(1)(2)); // RFC6145
    BOOST_CHECK(CNetAddr("64:FF9B::102:304").GetGroup() == boost::assign::list_of((unsigned char)NET_IPV4)(1)(2)); // RFC6052
    BOOST_CHECK(CNetAddr("2002:102:304:9999:9999:9999:9999:9999").GetGroup() == boost::assign::list_of((unsigned char)NET_IPV4)(1)(2)); // RFC3964
    BOOST_CHECK(CNetAddr("2001:0:9999:9999:9999:9999:FEFD:FCFB").GetGroup() == boost::assign::list_of((unsigned char)NET_IPV4)(1)(2)); // RFC4380
    BOOST_CHECK(CNetAddr("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").GetGroup() == boost::assign::list_of((unsigned char)NET_TOR)(239)); // Tor
    BOOST_CHECK(CNetAddr("2001:470:abcd:9999:9999:9999:9999:9999").GetGroup() == boost::assign::list_of((unsigned char)NET_IPV6)(32)(1)(4)(112)(175)); //he.net
    BOOST_CHECK(CNetAddr("2001:2001:9999:9999:9999:9999:9999:9999").GetGroup() == boost::assign::list_of((unsigned char)NET_IPV6)(32)(1)(32)(1)); //IPv6
}

BOOST_AUTO_TEST_SUITE_END()
