/*
 *  Copyright (c) 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kis_painter_test.h"
#include <QTest>


#include <kis_debug.h>
#include <QRect>
#include <QTime>
#include <QtXml>

#include <KoChannelInfo.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoCompositeOpRegistry.h>

#include "kis_datamanager.h"
#include "kis_types.h"
#include "kis_paint_device.h"
#include "kis_painter.h"
#include "kis_pixel_selection.h"
#include "kis_fill_painter.h"
#include <kis_fixed_paint_device.h>
#include "testutil.h"
#include <kis_iterator_ng.h>

void KisPainterTest::allCsApplicator(void (KisPainterTest::* funcPtr)(const KoColorSpace*cs))
{
    QList<const KoColorSpace*> colorsapces = KoColorSpaceRegistry::instance()->allColorSpaces(KoColorSpaceRegistry::AllColorSpaces, KoColorSpaceRegistry::OnlyDefaultProfile);

    Q_FOREACH (const KoColorSpace* cs, colorsapces) {

        QString csId = cs->id();
        // ALL THESE COLORSPACES ARE BROKEN: WE NEED UNITTESTS FOR COLORSPACES!
        if (csId.startsWith("KS")) continue;
        if (csId.startsWith("Xyz")) continue;
        if (csId.startsWith('Y')) continue;
        if (csId.contains("AF")) continue;
        if (csId == "GRAYU16") continue; // No point in testing bounds with a cs without alpha
        if (csId == "GRAYU8") continue; // No point in testing bounds with a cs without alpha

        dbgKrita << "Testing with cs" << csId;

        if (cs && cs->compositeOp(COMPOSITE_OVER) != 0) {
            (this->*funcPtr)(cs);
        } else {
            dbgKrita << "Cannot bitBlt for cs" << csId;
        }
    }
}

void KisPainterTest::testSimpleBlt(const KoColorSpace * cs)
{

    KisPaintDeviceSP dst = new KisPaintDevice(cs);
    KisPaintDeviceSP src = new KisPaintDevice(cs);
    KoColor c(Qt::red, cs);
    c.setOpacity(quint8(128));
    src->fill(20, 20, 20, 20, c.data());

    QCOMPARE(src->exactBounds(), QRect(20, 20, 20, 20));

    const KoCompositeOp* op;

    {
        op = cs->compositeOp(COMPOSITE_OVER);
        KisPainter painter(dst);
        painter.setCompositeOp(op);
        painter.bitBlt(50, 50, src, 20, 20, 20, 20);
        painter.end();
        QCOMPARE(dst->exactBounds(), QRect(50,50,20,20));
    }

    dst->clear();

    {
        op = cs->compositeOp(COMPOSITE_COPY);
        KisPainter painter(dst);
        painter.setCompositeOp(op);
        painter.bitBlt(50, 50, src, 20, 20, 20, 20);
        painter.end();
        QCOMPARE(dst->exactBounds(), QRect(50,50,20,20));
    }
}

void KisPainterTest::testSimpleBlt()
{
    allCsApplicator(&KisPainterTest::testSimpleBlt);
}

/*

Note: the bltSelection tests assume the following geometry:

0,0               0,30
  +---------+------+
  |  10,10  |      |
  |    +----+      |
  |    |####|      |
  |    |####|      |
  +----+----+      |
  |       20,20    |
  |                |
  |                |
  +----------------+
                  30,30
 */
void KisPainterTest::testPaintDeviceBltSelection(const KoColorSpace * cs)
{

    KisPaintDeviceSP dst = new KisPaintDevice(cs);

    KisPaintDeviceSP src = new KisPaintDevice(cs);
    KoColor c(Qt::red, cs);
    c.setOpacity(quint8(128));
    src->fill(0, 0, 20, 20, c.data());

    QCOMPARE(src->exactBounds(), QRect(0, 0, 20, 20));

    KisSelectionSP selection = new KisSelection();
    selection->pixelSelection()->select(QRect(10, 10, 20, 20));
    selection->updateProjection();
    QCOMPARE(selection->selectedExactRect(), QRect(10, 10, 20, 20));

    KisPainter painter(dst);
    painter.setSelection(selection);

    painter.bitBlt(0, 0, src, 0, 0, 30, 30);
    painter.end();

    QImage image = dst->convertToQImage(0);
    image.save("blt_Selection_" + cs->name() + ".png");

    QCOMPARE(dst->exactBounds(), QRect(10, 10, 10, 10));

    const KoCompositeOp* op = cs->compositeOp(COMPOSITE_SUBTRACT);
    if (op->id() == COMPOSITE_SUBTRACT) {

        KisPaintDeviceSP dst2 = new KisPaintDevice(cs);
        KisPainter painter2(dst2);
        painter2.setSelection(selection);
        painter2.setCompositeOp(op);
        painter2.bitBlt(0, 0, src, 0, 0, 30, 30);
        painter2.end();

        QCOMPARE(dst2->exactBounds(), QRect(10, 10, 10, 10));
    }
}

void KisPainterTest::testPaintDeviceBltSelection()
{
    allCsApplicator(&KisPainterTest::testPaintDeviceBltSelection);
}

void KisPainterTest::testPaintDeviceBltSelectionIrregular(const KoColorSpace * cs)
{

    KisPaintDeviceSP dst = new KisPaintDevice(cs);
    KisPaintDeviceSP src = new KisPaintDevice(cs);
    KisFillPainter gc(src);
    gc.fillRect(0, 0, 20, 20, KoColor(Qt::red, cs));
    gc.end();

    QCOMPARE(src->exactBounds(), QRect(0, 0, 20, 20));

    KisSelectionSP sel = new KisSelection();

    KisPixelSelectionSP psel = sel->pixelSelection();
    psel->select(QRect(10, 15, 20, 15));
    psel->select(QRect(15, 10, 15, 5));

    QCOMPARE(psel->selectedExactRect(), QRect(10, 10, 20, 20));
    QCOMPARE(TestUtil::alphaDevicePixel(psel, 13, 13), MIN_SELECTED);

    KisPainter painter(dst);
    painter.setSelection(sel);
    painter.bitBlt(0, 0, src, 0, 0, 30, 30);
    painter.end();

    QImage image = dst->convertToQImage(0);
    image.save("blt_Selection_irregular" + cs->name() + ".png");

    QCOMPARE(dst->exactBounds(), QRect(10, 10, 10, 10));
    Q_FOREACH (KoChannelInfo * channel, cs->channels()) {
        // Only compare alpha if there actually is an alpha channel in
        // this colorspace
        if (channel->channelType() == KoChannelInfo::ALPHA) {
            QColor c;

            dst->pixel(13, 13, &c);

            QCOMPARE((int) c.alpha(), (int) OPACITY_TRANSPARENT_U8);
        }
    }
}


void KisPainterTest::testPaintDeviceBltSelectionIrregular()
{
    allCsApplicator(&KisPainterTest::testPaintDeviceBltSelectionIrregular);
}

void KisPainterTest::testPaintDeviceBltSelectionInverted(const KoColorSpace * cs)
{

    KisPaintDeviceSP dst = new KisPaintDevice(cs);
    KisPaintDeviceSP src = new KisPaintDevice(cs);
    KisFillPainter gc(src);
    gc.fillRect(0, 0, 30, 30, KoColor(Qt::red, cs));
    gc.end();
    QCOMPARE(src->exactBounds(), QRect(0, 0, 30, 30));

    KisSelectionSP sel = new KisSelection();
    KisPixelSelectionSP psel = sel->pixelSelection();
    psel->select(QRect(10, 10, 20, 20));
    psel->invert();
    sel->updateProjection();

    KisPainter painter(dst);
    painter.setSelection(sel);
    painter.bitBlt(0, 0, src, 0, 0, 30, 30);
    painter.end();
    QCOMPARE(dst->exactBounds(), QRect(0, 0, 30, 30));
}

void KisPainterTest::testPaintDeviceBltSelectionInverted()
{
    allCsApplicator(&KisPainterTest::testPaintDeviceBltSelectionInverted);
}


void KisPainterTest::testSelectionBltSelection()
{
    KisPixelSelectionSP src = new KisPixelSelection();
    src->select(QRect(0, 0, 20, 20));
    QCOMPARE(src->selectedExactRect(), QRect(0, 0, 20, 20));

    KisSelectionSP sel = new KisSelection();

    KisPixelSelectionSP Selection = sel->pixelSelection();
    Selection->select(QRect(10, 10, 20, 20));
    QCOMPARE(Selection->selectedExactRect(), QRect(10, 10, 20, 20));

    sel->updateProjection();
    KisPixelSelectionSP dst = new KisPixelSelection();
    KisPainter painter(dst);
    painter.setSelection(sel);
    painter.bitBlt(0, 0, src, 0, 0, 30, 30);
    painter.end();

    QCOMPARE(dst->selectedExactRect(), QRect(10, 10, 10, 10));

    KisSequentialConstIterator it(dst, QRect(10, 10, 10, 10));
    do {
        // These are selections, so only one channel and it should
        // be totally selected
        QCOMPARE(it.oldRawData()[0], MAX_SELECTED);
    } while (it.nextPixel());
}

/*

Test with non-square selection

0,0               0,30
  +-----------+------+
  |    13,13  |      |
  |      x +--+      |
  |     +--+##|      |
  |     |#####|      |
  +-----+-----+      |
  |         20,20    |
  |                  |
  |                  |
  +------------------+
                  30,30
 */
void KisPainterTest::testSelectionBltSelectionIrregular()
{

    KisPaintDeviceSP dev = new KisPaintDevice(KoColorSpaceRegistry::instance()->rgb8());

    KisPixelSelectionSP src = new KisPixelSelection();
    src->select(QRect(0, 0, 20, 20));
    QCOMPARE(src->selectedExactRect(), QRect(0, 0, 20, 20));

    KisSelectionSP sel = new KisSelection();

    KisPixelSelectionSP Selection = sel->pixelSelection();
    Selection->select(QRect(10, 15, 20, 15));
    Selection->select(QRect(15, 10, 15, 5));
    QCOMPARE(Selection->selectedExactRect(), QRect(10, 10, 20, 20));
    QCOMPARE(TestUtil::alphaDevicePixel(Selection, 13, 13), MIN_SELECTED);

    sel->updateProjection();

    KisPixelSelectionSP dst = new KisPixelSelection();
    KisPainter painter(dst);
    painter.setSelection(sel);
    painter.bitBlt(0, 0, src, 0, 0, 30, 30);
    painter.end();

    QCOMPARE(dst->selectedExactRect(), QRect(10, 10, 10, 10));
    QCOMPARE(TestUtil::alphaDevicePixel(dst, 13, 13), MIN_SELECTED);
}

void KisPainterTest::testSelectionBitBltFixedSelection()
{
    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP dst = new KisPaintDevice(cs);

    KisPaintDeviceSP src = new KisPaintDevice(cs);
    KoColor c(Qt::red, cs);
    c.setOpacity(quint8(128));
    src->fill(0, 0, 20, 20, c.data());

    QCOMPARE(src->exactBounds(), QRect(0, 0, 20, 20));

    KisFixedPaintDeviceSP fixedSelection = new KisFixedPaintDevice(cs);
    fixedSelection->setRect(QRect(0, 0, 20, 20));
    fixedSelection->initialize();
    KoColor fill(Qt::white, cs);
    fixedSelection->fill(5, 5, 10, 10, fill.data());
    fixedSelection->convertTo(KoColorSpaceRegistry::instance()->alpha8());

    KisPainter painter(dst);

    painter.bitBltWithFixedSelection(0, 0, src, fixedSelection, 20, 20);
    painter.end();

    QCOMPARE(dst->exactBounds(), QRect(5, 5, 10, 10));
    /*
dbgKrita << "canary1.5";
    dst->clear();
    painter.begin(dst);

    painter.bitBltWithFixedSelection(0, 0, src, fixedSelection, 10, 20);
    painter.end();
dbgKrita << "canary2";
    QCOMPARE(dst->exactBounds(), QRect(5, 5, 5, 10));

    dst->clear();
    painter.begin(dst);

    painter.bitBltWithFixedSelection(0, 0, src, fixedSelection, 5, 5, 5, 5, 10, 20);
    painter.end();
dbgKrita << "canary3";
    QCOMPARE(dst->exactBounds(), QRect(5, 5, 5, 10));

    dst->clear();
    painter.begin(dst);

    painter.bitBltWithFixedSelection(5, 5, src, fixedSelection, 10, 20);
    painter.end();
dbgKrita << "canary4";
    QCOMPARE(dst->exactBounds(), QRect(10, 10, 5, 10));
    */
}

void KisPainterTest::testSelectionBitBltEraseCompositeOp()
{
    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP dst = new KisPaintDevice(cs);
    KoColor c(Qt::red, cs);
    dst->fill(0, 0, 150, 150, c.data());

    KisPaintDeviceSP src = new KisPaintDevice(cs);
    KoColor c2(Qt::black, cs);
    src->fill(50, 50, 50, 50, c2.data());

    KisSelectionSP sel = new KisSelection();
    KisPixelSelectionSP selection = sel->pixelSelection();
    selection->select(QRect(25, 25, 100, 100));
    sel->updateProjection();

    const KoCompositeOp* op = cs->compositeOp(COMPOSITE_ERASE);
    KisPainter painter(dst);
    painter.setSelection(sel);
    painter.setCompositeOp(op);
    painter.bitBlt(0, 0, src, 0, 0, 150, 150);
    painter.end();

    //dst->convertToQImage(0).save("result.png");

    QRect erasedRect(50, 50, 50, 50);
    KisSequentialConstIterator it(dst, QRect(0, 0, 150, 150));
    do {
        if(!erasedRect.contains(it.x(), it.y())) {
             QVERIFY(memcmp(it.oldRawData(), c.data(), cs->pixelSize()) == 0);
        }
    } while (it.nextPixel());

}

void KisPainterTest::testSimpleAlphaCopy()
{
    KisPaintDeviceSP src = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8());
    KisPaintDeviceSP dst = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8());
    quint8 p = 128;
    src->fill(0, 0, 100, 100, &p);
    QVERIFY(src->exactBounds() == QRect(0, 0, 100, 100));
    KisPainter gc(dst);
    gc.setCompositeOp(KoColorSpaceRegistry::instance()->alpha8()->compositeOp(COMPOSITE_COPY));
    gc.bitBlt(QPoint(0, 0), src, src->exactBounds());
    gc.end();
    QCOMPARE(dst->exactBounds(), QRect(0, 0, 100, 100));

}

void KisPainterTest::checkPerformance()
{
    KisPaintDeviceSP src = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8());
    KisPaintDeviceSP dst = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8());
    quint8 p = 128;
    src->fill(0, 0, 10000, 5000, &p);
    KisSelectionSP sel = new KisSelection();
    sel->pixelSelection()->select(QRect(0, 0, 10000, 5000), 128);
    sel->updateProjection();

    QTime t;
    t.start();
    for (int i = 0; i < 10; ++i) {
        KisPainter gc(dst);
        gc.bitBlt(0, 0, src, 0, 0, 10000, 5000);
    }

    t.restart();
    for (int i = 0; i < 10; ++i) {
        KisPainter gc(dst, sel);
        gc.bitBlt(0, 0, src, 0, 0, 10000, 5000);
    }
}

void KisPainterTest::testBitBltOldData()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->alpha8();

    KisPaintDeviceSP src = new KisPaintDevice(cs);
    KisPaintDeviceSP dst = new KisPaintDevice(cs);

    quint8 defaultPixel = 0;
    quint8 p1 = 128;
    quint8 p2 = 129;
    quint8 p3 = 130;
    KoColor defaultColor(&defaultPixel, cs);
    KoColor color1(&p1, cs);
    KoColor color2(&p2, cs);
    KoColor color3(&p3, cs);
    QRect fillRect(0,0,5000,5000);

    src->fill(fillRect, color1);

    KisPainter srcGc(src);
    srcGc.beginTransaction();
    src->fill(fillRect, color2);

    KisPainter dstGc(dst);
    dstGc.bitBltOldData(QPoint(), src, fillRect);

    QVERIFY(TestUtil::checkAlphaDeviceFilledWithPixel(dst, fillRect, p1));

    dstGc.end();
    srcGc.deleteTransaction();
}

void KisPainterTest::benchmarkBitBlt()
{
    quint8 p = 128;
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->alpha8();

    KisPaintDeviceSP src = new KisPaintDevice(cs);
    KisPaintDeviceSP dst = new KisPaintDevice(cs);

    KoColor color(&p, cs);
    QRect fillRect(0,0,5000,5000);

    src->fill(fillRect, color);

    QBENCHMARK {
        KisPainter gc(dst);
        gc.bitBlt(QPoint(), src, fillRect);
    }
}

void KisPainterTest::benchmarkBitBltOldData()
{
    quint8 p = 128;
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->alpha8();

    KisPaintDeviceSP src = new KisPaintDevice(cs);
    KisPaintDeviceSP dst = new KisPaintDevice(cs);

    KoColor color(&p, cs);
    QRect fillRect(0,0,5000,5000);

    src->fill(fillRect, color);

    QBENCHMARK {
        KisPainter gc(dst);
        gc.bitBltOldData(QPoint(), src, fillRect);
    }
}
#include "kis_paint_device_debug_utils.h"
#include "KisRenderedDab.h"

void testMassiveBltFixedImpl(int numRects, bool varyOpacity = false, bool useSelection = false)
{
    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP dst = new KisPaintDevice(cs);

    QList<QColor> colors;
    colors << Qt::red;
    colors << Qt::green;
    colors << Qt::blue;

    QRect devicesRect;
    QList<KisRenderedDab> devices;

    for (int i = 0; i < numRects; i++) {
        const QRect rc(10 + i * 10, 10 + i * 10, 30, 30);
        KisFixedPaintDeviceSP dev = new KisFixedPaintDevice(cs);
        dev->setRect(rc);
        dev->initialize();
        dev->fill(rc, KoColor(colors[i % 3], cs));
        dev->fill(kisGrowRect(rc, -5), KoColor(Qt::white, cs));

        KisRenderedDab dab;
        dab.device = dev;
        dab.offset = dev->bounds().topLeft();
        dab.opacity = varyOpacity ? qreal(1 + i) / numRects : 1.0;
        dab.flow = 1.0;

        devices << dab;
        devicesRect |= rc;
    }

    KisSelectionSP selection;

    if (useSelection) {
        selection = new KisSelection();
        selection->pixelSelection()->select(kisGrowRect(devicesRect, -7));
    }

    const QString opacityPostfix = varyOpacity ? "_varyop" : "";
    const QString selectionPostfix = useSelection ? "_sel" : "";

    const QRect fullRect = kisGrowRect(devicesRect, 10);

    {
        KisPainter painter(dst);
        painter.setSelection(selection);
        painter.bltFixed(fullRect, devices);
        painter.end();
        QVERIFY(TestUtil::checkQImage(dst->convertToQImage(0, fullRect),
                                      "kispainter_test",
                                      "massive_bitblt",
                                      QString("full_update_%1%2%3")
                                          .arg(numRects)
                                          .arg(opacityPostfix)
                                          .arg(selectionPostfix)));
    }

    dst->clear();

    {
        KisPainter painter(dst);
        painter.setSelection(selection);

        for (int i = fullRect.x(); i <= fullRect.center().x(); i += 10) {
            const QRect rc(i, fullRect.y(), 10, fullRect.height());
            painter.bltFixed(rc, devices);
        }

        painter.end();

        QVERIFY(TestUtil::checkQImage(dst->convertToQImage(0, fullRect),
                                      "kispainter_test",
                                      "massive_bitblt",
                                      QString("partial_update_%1%2%3")
                                          .arg(numRects)
                                          .arg(opacityPostfix)
                                          .arg(selectionPostfix)));

    }
}

void KisPainterTest::testMassiveBltFixedSingleTile()
{
    testMassiveBltFixedImpl(3);
}

void KisPainterTest::testMassiveBltFixedMultiTile()
{
    testMassiveBltFixedImpl(6);
}

void KisPainterTest::testMassiveBltFixedMultiTileWithOpacity()
{
    testMassiveBltFixedImpl(6, true);
}

void KisPainterTest::testMassiveBltFixedMultiTileWithSelection()
{
    testMassiveBltFixedImpl(6, false, true);
}

void KisPainterTest::testMassiveBltFixedCornerCases()
{
    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP dst = new KisPaintDevice(cs);

    QList<KisRenderedDab> devices;

    QVERIFY(dst->extent().isEmpty());

    {
        // empty devices, shouldn't crash
        KisPainter painter(dst);
        painter.bltFixed(QRect(60,60,20,20), devices);
        painter.end();
    }

    QVERIFY(dst->extent().isEmpty());

    const QRect rc(10,10,20,20);
    KisFixedPaintDeviceSP dev = new KisFixedPaintDevice(cs);
    dev->setRect(rc);
    dev->initialize();
    dev->fill(rc, KoColor(Qt::white, cs));

    devices.append(KisRenderedDab(dev));

    {
        // rect outside the devices bounds, shouldn't crash
        KisPainter painter(dst);
        painter.bltFixed(QRect(60,60,20,20), devices);
        painter.end();
    }

    QVERIFY(dst->extent().isEmpty());
}


#include "kis_paintop_utils.h"
#include "kis_algebra_2d.h"

void benchmarkMassiveBltFixedImpl(int numDabs, int size, qreal spacing, int idealNumPatches, Qt::Orientations direction)
{
    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP dst = new KisPaintDevice(cs);

    QList<QColor> colors;
    colors << QColor(255, 0, 0, 200);
    colors << QColor(0, 255, 0, 200);
    colors << QColor(0, 0, 255, 200);

    QRect devicesRect;
    QList<KisRenderedDab> devices;

    const int step = spacing * size;

    for (int i = 0; i < numDabs; i++) {
        const QRect rc =
            direction == Qt::Horizontal ? QRect(10 + i * step, 0, size, size) :
            direction == Qt::Vertical ? QRect(0, 10 + i * step, size, size) :
            QRect(10 + i * step, 10 + i * step, size, size);

        KisFixedPaintDeviceSP dev = new KisFixedPaintDevice(cs);
        dev->setRect(rc);
        dev->initialize();
        dev->fill(rc, KoColor(colors[i % 3], cs));
        dev->fill(kisGrowRect(rc, -5), KoColor(Qt::white, cs));

        KisRenderedDab dab;
        dab.device = dev;
        dab.offset = dev->bounds().topLeft();
        dab.opacity = 1.0;
        dab.flow = 1.0;

        devices << dab;
        devicesRect |= rc;
    }

    const QRect fullRect = kisGrowRect(devicesRect, 10);

    {
        KisPainter painter(dst);
        painter.bltFixed(fullRect, devices);
        painter.end();
        //QVERIFY(TestUtil::checkQImage(dst->convertToQImage(0, fullRect),
        //                              "kispainter_test",
        //                              "massive_bitblt_benchmark",
        //                              "initial"));
        dst->clear();
    }


    QVector<QRect> dabRects;
    Q_FOREACH (const KisRenderedDab &dab, devices) {
        dabRects.append(dab.realBounds());
    }

    QElapsedTimer t;

    qint64 massiveTime = 0;
    int massiveTries = 0;
    int numRects = 0;
    int avgPatchSize = 0;

    for (int i = 0; i < 50 || massiveTime > 5000000; i++) {
        QVector<QRect> rects = KisPaintOpUtils::splitDabsIntoRects(dabRects, idealNumPatches, size, spacing);
        numRects = rects.size();

        // HACK: please calculate real *average*!
        avgPatchSize = KisAlgebra2D::maxDimension(rects.first());

        t.start();

        KisPainter painter(dst);
        Q_FOREACH (const QRect &rc, rects) {
            painter.bltFixed(rc, devices);
        }
        painter.end();

        massiveTime += t.nsecsElapsed() / 1000;
        massiveTries++;
        dst->clear();
    }

    qint64 linearTime = 0;
    int linearTries = 0;

    for (int i = 0; i < 50 || linearTime > 5000000; i++) {
        t.start();

        KisPainter painter(dst);
        Q_FOREACH (const KisRenderedDab &dab, devices) {
            painter.setOpacity(255 * dab.opacity);
            painter.setFlow(255 * dab.flow);
            painter.bltFixed(dab.offset, dab.device, dab.device->bounds());
        }
        painter.end();

        linearTime += t.nsecsElapsed() / 1000;
        linearTries++;
        dst->clear();
    }

    const qreal avgMassive = qreal(massiveTime) / massiveTries;
    const qreal avgLinear = qreal(linearTime) / linearTries;

    const QString directionMark =
        direction == Qt::Horizontal ? "H" :
        direction == Qt::Vertical ? "V" : "D";

    qDebug()
            << "D:" << size
            << "S:" << spacing
            << "N:" << numDabs
            << "P (px):" << avgPatchSize
            << "R:" << numRects
            << "Dir:" << directionMark
            << "\t"
            << qPrintable(QString("Massive (usec): %1").arg(QString::number(avgMassive, 'f', 2), 8))
            << "\t"
            << qPrintable(QString("Linear (usec): %1").arg(QString::number(avgLinear, 'f', 2), 8))
            << (avgMassive < avgLinear ? "*" : " ")
            << qPrintable(QString("%1")
                   .arg(QString::number((avgMassive - avgLinear) / avgLinear * 100.0, 'f', 2), 8))
            << qRound(size + size * spacing * (numDabs - 1));
}


void KisPainterTest::benchmarkMassiveBltFixed()
{
    const qreal sp = 0.14;
    const int idealThreadCount = 8;

    for (int d = 50; d < 301; d += 50) {
        for (int n = 1; n < 150; n = qCeil(n * 1.5)) {
            benchmarkMassiveBltFixedImpl(n, d, sp, idealThreadCount, Qt::Horizontal);
            benchmarkMassiveBltFixedImpl(n, d, sp, idealThreadCount, Qt::Vertical);
            benchmarkMassiveBltFixedImpl(n, d, sp, idealThreadCount, Qt::Vertical | Qt::Horizontal);
        }
    }
}

QTEST_MAIN(KisPainterTest)


