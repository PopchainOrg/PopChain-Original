// Copyright (c) 2017-2018 The Popchain Core Developers

#include "uritests.h"

#include "guiutil.h"
#include "walletmodel.h"

#include <QUrl>

void URITests::uriTests()
{
    SendCoinsRecipient rv;
    QUrl uri;
    uri.setUrl(QString("pop:PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ ?req-dontexist="));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("pop:PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ ?dontexist="));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ "));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("pop:PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ ?label=Some Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ "));
    QVERIFY(rv.label == QString("Some Example Address"));
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("pop:PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ ?amount=0.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ "));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100000);

    uri.setUrl(QString("pop:PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ ?amount=1.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ "));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100100000);

    uri.setUrl(QString("pop:PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ ?amount=100&label=Some Example"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ "));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("Some Example"));

    uri.setUrl(QString("pop:PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ ?message=Some Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ "));
    QVERIFY(rv.label == QString());

    QVERIFY(GUIUtil::parseBitcoinURI("pop://PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ ?message=Some Example Address", &rv));
    QVERIFY(rv.address == QString("PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ "));
    QVERIFY(rv.label == QString());

    uri.setUrl(QString("pop:PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ ?req-message=Some Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("pop:PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ ?amount=1,000&label=Some Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("pop:PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ ?amount=1,000.0&label=Some Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("pop:PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ ?amount=100&label=Some Example&message=Some Example Message&IS=1"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ "));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("Some Example"));
    QVERIFY(rv.message == QString("Some Example Message"));
    QVERIFY(rv.fUseInstantSend == 1);

    uri.setUrl(QString("pop:PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ ?amount=100&label=Some Example&message=Some Example Message&IS=Something Invalid"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ "));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("Some Example"));
    QVERIFY(rv.message == QString("Some Example Message"));
    QVERIFY(rv.fUseInstantSend != 1);

    uri.setUrl(QString("pop:PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ ?IS=1"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.fUseInstantSend == 1);

    uri.setUrl(QString("pop:PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ ?IS=0"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.fUseInstantSend != 1);

    uri.setUrl(QString("pop:PmWShCPNb2SvThFvzAH5jpNGPUPqDVc5fJ "));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.fUseInstantSend != 1);
}
