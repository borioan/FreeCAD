/***************************************************************************
 *   Copyright (c) 2021 Abdullah Tahiri <abdullah.tahiri.yo@gmail.com>     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"

#include <Base/Console.h>
#include <Base/Exception.h>

#include "EditModeCoinManagerParameters.h"
#include "EditModeGeometryCoinConverter.h"
#include "Utils.h"
#include "ViewProviderSketchCoinAttorney.h"
#include "ViewProviderSketchGeometryExtension.h"


using namespace SketcherGui;

EditModeGeometryCoinConverter::EditModeGeometryCoinConverter(
    ViewProviderSketch& vp,
    GeometryLayerNodes& geometrylayernodes,
    DrawingParameters& drawingparameters,
    GeometryLayerParameters& geometryLayerParams,
    CoinMapping& coinMap)
    : viewProvider(vp)
    , geometryLayerNodes(geometrylayernodes)
    , drawingParameters(drawingparameters)
    , geometryLayerParameters(geometryLayerParams)
    , coinMapping(coinMap)
{}

void EditModeGeometryCoinConverter::convert(const Sketcher::GeoListFacade& geolistfacade)
{

    // measurements
    bsplineGeoIds.clear();
    arcGeoIds.clear();

    // end information layer
    Points.clear();
    Coords.clear();
    Index.clear();

    coinMapping.clear();

    pointCounter.clear();

    for (auto l = 0; l < geometryLayerParameters.getCoinLayerCount(); l++) {
        Points.emplace_back();
        Coords.emplace_back();
        Index.emplace_back();

        coinMapping.CurvIdToGeoId.emplace_back();
        for (int t = 0; t < geometryLayerParameters.getSubLayerCount(); t++) {
            Coords[l].emplace_back();
            Index[l].emplace_back();
            coinMapping.CurvIdToGeoId[l].emplace_back();
        }
        coinMapping.PointIdToGeoId.emplace_back();
        coinMapping.PointIdToPosId.emplace_back();
        coinMapping.PointIdToVertexId.emplace_back();
    }

    pointCounter.resize(geometryLayerParameters.getCoinLayerCount(), 0);

    // RootPoint
    // TODO: RootPoint is here added in layer0. However, this layer may be hidden. The point should,
    // when that functionality is provided, be added to the first visible layer, or may even a new
    // empty layer.
    Points[0].emplace_back(0., 0., 0.);
    coinMapping.PointIdToGeoId[0].push_back(-1);  // root point
    coinMapping.PointIdToPosId[0].push_back(Sketcher::PointPos::start);
    coinMapping.PointIdToVertexId[0].push_back(-1);
    // VertexId is the reference used for point selection/preselection

    coinMapping.GeoElementId2SetId.emplace(std::piecewise_construct,
                                           std::forward_as_tuple(Sketcher::GeoElementId::RtPnt),
                                           std::forward_as_tuple(pointCounter[0]++, 0));

    auto setTracking = [this](int geoId,
                              int coinLayer,
                              EditModeGeometryCoinConverter::PointsMode pointmode,
                              int numberCurves,
                              int sublayer) {
        int numberPoints = 0;

        if (pointmode == PointsMode::InsertSingle) {
            numberPoints = 1;

            coinMapping.GeoElementId2SetId.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(geoId, Sketcher::PointPos::start),
                std::forward_as_tuple(pointCounter[coinLayer]++, coinLayer));
        }
        else if (pointmode == PointsMode::InsertStartEnd) {
            numberPoints = 2;

            coinMapping.GeoElementId2SetId.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(geoId, Sketcher::PointPos::start),
                std::forward_as_tuple(pointCounter[coinLayer]++, coinLayer));

            coinMapping.GeoElementId2SetId.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(geoId, Sketcher::PointPos::end),
                std::forward_as_tuple(pointCounter[coinLayer]++, coinLayer));
        }
        else if (pointmode == PointsMode::InsertMidOnly) {
            numberPoints = 1;

            coinMapping.GeoElementId2SetId.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(geoId, Sketcher::PointPos::mid),
                std::forward_as_tuple(pointCounter[coinLayer]++, coinLayer));
        }
        else if (pointmode == PointsMode::InsertStartEndMid) {
            numberPoints = 3;

            coinMapping.GeoElementId2SetId.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(geoId, Sketcher::PointPos::start),
                std::forward_as_tuple(pointCounter[coinLayer]++, coinLayer));

            coinMapping.GeoElementId2SetId.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(geoId, Sketcher::PointPos::end),
                std::forward_as_tuple(pointCounter[coinLayer]++, coinLayer));

            coinMapping.GeoElementId2SetId.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(geoId, Sketcher::PointPos::mid),
                std::forward_as_tuple(pointCounter[coinLayer]++, coinLayer));
        }

        for (int i = 0; i < numberPoints; i++) {
            coinMapping.PointIdToGeoId[coinLayer].push_back(geoId);
            Sketcher::PointPos pos;
            if (i == 0) {
                if (pointmode == PointsMode::InsertMidOnly) {
                    pos = Sketcher::PointPos::mid;
                }
                else {
                    pos = Sketcher::PointPos::start;
                }
            }
            else if (i == 1) {
                pos = Sketcher::PointPos::end;
            }
            else {
                pos = Sketcher::PointPos::mid;
            }

            coinMapping.PointIdToPosId[coinLayer].push_back(pos);
            coinMapping.PointIdToVertexId[coinLayer].push_back(vertexCounter++);
        }

        if (numberCurves > 0) {  // insert the first segment of the curve into the map
            coinMapping.GeoElementId2SetId.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(geoId, Sketcher::PointPos::none),
                std::forward_as_tuple(
                    static_cast<int>(coinMapping.CurvIdToGeoId[coinLayer][sublayer].size()),
                    coinLayer,
                    sublayer));
        }

        for (int i = 0; i < numberCurves; i++) {
            coinMapping.CurvIdToGeoId[coinLayer][sublayer].push_back(geoId);
        }
    };

    for (size_t i = 0; i < geolistfacade.geomlist.size() - 2; i++) {

        const auto GeoId = geolistfacade.getGeoIdFromGeomListIndex(i);
        const auto geom = geolistfacade.getGeometryFacadeFromGeoId(GeoId);
        const auto type = geom->getGeometry()->getTypeId();

        int layerId = getSafeGeomLayerId(geom);
        int subLayerId = geometryLayerParameters.getSubLayerIndex(GeoId, geom);

        auto coinLayer = geometryLayerParameters.getSafeCoinLayer(layerId);

        if (type == Part::GeomPoint::getClassTypeId()) {  // add a point
            convert<Part::GeomPoint,
                    EditModeGeometryCoinConverter::PointsMode::InsertSingle,
                    EditModeGeometryCoinConverter::CurveMode::NoCurve,
                    EditModeGeometryCoinConverter::AnalyseMode::BoundingBoxMagnitude>(geom,
                                                                                      GeoId,
                                                                                      subLayerId);
            setTracking(GeoId,
                        coinLayer,
                        EditModeGeometryCoinConverter::PointsMode::InsertSingle,
                        0,
                        subLayerId);
        }
        else if (type == Part::GeomLineSegment::getClassTypeId()) {  // add a line
            convert<Part::GeomLineSegment,
                    EditModeGeometryCoinConverter::PointsMode::InsertStartEnd,
                    EditModeGeometryCoinConverter::CurveMode::StartEndPointsOnly,
                    EditModeGeometryCoinConverter::AnalyseMode::BoundingBoxMagnitude>(geom,
                                                                                      GeoId,
                                                                                      subLayerId);
            setTracking(GeoId,
                        coinLayer,
                        EditModeGeometryCoinConverter::PointsMode::InsertStartEnd,
                        1,
                        subLayerId);
        }
        else if (type.isDerivedFrom(
                     Part::GeomConic::getClassTypeId())) {  // add a closed curve conic
            convert<Part::GeomConic,
                    EditModeGeometryCoinConverter::PointsMode::InsertMidOnly,
                    EditModeGeometryCoinConverter::CurveMode::ClosedCurve,
                    EditModeGeometryCoinConverter::AnalyseMode::BoundingBoxMagnitude>(geom,
                                                                                      GeoId,
                                                                                      subLayerId);
            setTracking(GeoId,
                        coinLayer,
                        EditModeGeometryCoinConverter::PointsMode::InsertMidOnly,
                        1,
                        subLayerId);
        }
        else if (type.isDerivedFrom(
                     Part::GeomArcOfConic::getClassTypeId())) {  // add an arc of conic
            convert<Part::GeomArcOfConic,
                    EditModeGeometryCoinConverter::PointsMode::InsertStartEndMid,
                    EditModeGeometryCoinConverter::CurveMode::OpenCurve,
                    EditModeGeometryCoinConverter::AnalyseMode::BoundingBoxMagnitude>(geom,
                                                                                      GeoId,
                                                                                      subLayerId);
            setTracking(GeoId,
                        coinLayer,
                        EditModeGeometryCoinConverter::PointsMode::InsertStartEndMid,
                        1,
                        subLayerId);
            arcGeoIds.push_back(GeoId);
        }
        else if (type == Part::GeomBSplineCurve::getClassTypeId()) {  // add a bspline (a bounded
                                                                      // curve that is not a conic)
            convert<Part::GeomBSplineCurve,
                    EditModeGeometryCoinConverter::PointsMode::InsertStartEnd,
                    EditModeGeometryCoinConverter::CurveMode::OpenCurve,
                    EditModeGeometryCoinConverter::AnalyseMode::
                        BoundingBoxMagnitudeAndBSplineCurvature>(geom, GeoId, subLayerId);
            setTracking(GeoId,
                        coinLayer,
                        EditModeGeometryCoinConverter::PointsMode::InsertStartEnd,
                        1,
                        subLayerId);
            bsplineGeoIds.push_back(GeoId);
        }
    }

    // Coin Nodes Editing
    int vOrFactor = ViewProviderSketchCoinAttorney::getViewOrientationFactor(viewProvider);
    double linez = vOrFactor * static_cast<double>(drawingParameters.zLowLines);  // NOLINT
    double pointz = vOrFactor * static_cast<double>(drawingParameters.zLowPoints);

    for (auto l = 0; l < geometryLayerParameters.getCoinLayerCount(); l++) {
        geometryLayerNodes.PointsCoordinate[l]->point.setNum(Points[l].size());
        geometryLayerNodes.PointsMaterials[l]->diffuseColor.setNum(Points[l].size());
        SbVec3f* pverts = geometryLayerNodes.PointsCoordinate[l]->point.startEditing();

        int i = 0;  // setting up the point set
        for (auto& point : Points[l]) {
            pverts[i++].setValue(point.x, point.y, pointz);
        }
        geometryLayerNodes.PointsCoordinate[l]->point.finishEditing();

        for (auto t = 0; t < geometryLayerParameters.getSubLayerCount(); t++) {
            geometryLayerNodes.CurvesCoordinate[l][t]->point.setNum(Coords[l][t].size());
            geometryLayerNodes.CurveSet[l][t]->numVertices.setNum(Index[l][t].size());
            geometryLayerNodes.CurvesMaterials[l][t]->diffuseColor.setNum(Index[l][t].size());

            SbVec3f* verts = geometryLayerNodes.CurvesCoordinate[l][t]->point.startEditing();
            int32_t* index = geometryLayerNodes.CurveSet[l][t]->numVertices.startEditing();

            i = 0;  // setting up the line set
            for (auto& coord : Coords[l][t]) {
                verts[i++].setValue(coord.x, coord.y, linez);  // NOLINT
            }

            i = 0;  // setting up the indexes of the line set
            for (auto it : Index[l][t]) {
                index[i++] = it;
            }

            geometryLayerNodes.CurvesCoordinate[l][t]->point.finishEditing();
            geometryLayerNodes.CurveSet[l][t]->numVertices.finishEditing();
        }
    }
}

template<typename GeoType,
         EditModeGeometryCoinConverter::PointsMode pointmode,
         EditModeGeometryCoinConverter::CurveMode curvemode,
         EditModeGeometryCoinConverter::AnalyseMode analysemode>
void EditModeGeometryCoinConverter::convert(const Sketcher::GeometryFacade* geometryfacade,
                                            [[maybe_unused]] int geoid,
                                            [[maybe_unused]] int subLayer)
{
    auto geo = static_cast<const GeoType*>(geometryfacade->getGeometry());
    auto layerId = getSafeGeomLayerId(geometryfacade);

    auto coinLayer = geometryLayerParameters.getSafeCoinLayer(layerId);

    auto addPoint = [&dMg = boundingBoxMaxMagnitude](auto& pushvector, Base::Vector3d point) {
        if constexpr (analysemode == AnalyseMode::BoundingBoxMagnitude
                      || analysemode == AnalyseMode::BoundingBoxMagnitudeAndBSplineCurvature) {
            dMg = dMg > std::abs(point.x) ? dMg : std::abs(point.x);
            dMg = dMg > std::abs(point.y) ? dMg : std::abs(point.y);
            pushvector.push_back(point);
        }
    };

    // Points
    if constexpr (pointmode == PointsMode::InsertSingle) {
        addPoint(Points[coinLayer], geo->getPoint());
    }
    else if constexpr (pointmode == PointsMode::InsertStartEnd) {
        addPoint(Points[coinLayer], geo->getStartPoint());
        addPoint(Points[coinLayer], geo->getEndPoint());
    }
    else if constexpr (pointmode == PointsMode::InsertStartEndMid) {
        // All in this group are Trimmed Curves (see Geometry.h)
        addPoint(Points[coinLayer], geo->getStartPoint(/*emulateCCW=*/true));
        addPoint(Points[coinLayer], geo->getEndPoint(/*emulateCCW=*/true));
        addPoint(Points[coinLayer], geo->getCenter());
    }
    else if constexpr (pointmode == PointsMode::InsertMidOnly) {
        addPoint(Points[coinLayer], geo->getCenter());
    }

    // Curves
    if constexpr (curvemode == CurveMode::StartEndPointsOnly) {
        addPoint(Coords[coinLayer][subLayer], geo->getStartPoint());
        addPoint(Coords[coinLayer][subLayer], geo->getEndPoint());
        Index[coinLayer][subLayer].push_back(2);
    }
    else if constexpr (curvemode == CurveMode::ClosedCurve) {
        int numSegments = drawingParameters.curvedEdgeCountSegments;
        if constexpr (std::is_same<GeoType, Part::GeomBSplineCurve>::value) {
            numSegments *= geo->countKnots();
        }

        double segment = (geo->getLastParameter() - geo->getFirstParameter()) / numSegments;

        for (int i = 0; i < numSegments; i++) {
            Base::Vector3d pnt = geo->value(i * segment);
            addPoint(Coords[coinLayer][subLayer], pnt);
        }

        Base::Vector3d pnt = geo->value(0);
        addPoint(Coords[coinLayer][subLayer], pnt);

        Index[coinLayer][subLayer].push_back(numSegments + 1);
    }
    else if constexpr (curvemode == CurveMode::OpenCurve) {
        int numSegments = drawingParameters.curvedEdgeCountSegments;
        if constexpr (std::is_same<GeoType, Part::GeomBSplineCurve>::value) {
            numSegments *= (geo->countKnots() - 1);  // one less segments than knots
        }

        double segment = (geo->getLastParameter() - geo->getFirstParameter()) / numSegments;

        for (int i = 0; i < numSegments; i++) {
            Base::Vector3d pnt = geo->value(geo->getFirstParameter() + i * segment);
            addPoint(Coords[coinLayer][subLayer], pnt);
        }

        Base::Vector3d pnt = geo->value(geo->getLastParameter());
        addPoint(Coords[coinLayer][subLayer], pnt);

        Index[coinLayer][subLayer].push_back(numSegments + 1);

        if constexpr (analysemode == AnalyseMode::BoundingBoxMagnitudeAndBSplineCurvature) {
            //***************************************************************************************************************
            // global information gathering for geometry information layer

            std::vector<Base::Vector3d> poles = geo->getPoles();

            Base::Vector3d midp = Base::Vector3d(0, 0, 0);

            for (std::vector<Base::Vector3d>::iterator it = poles.begin(); it != poles.end();
                 ++it) {
                midp += (*it);
            }

            midp /= poles.size();

            double firstparam = geo->getFirstParameter();
            double lastparam = geo->getLastParameter();

            const int ndiv = poles.size() > 4 ? poles.size() * 16 : 64;
            double step = (lastparam - firstparam) / (ndiv - 1);

            std::vector<double> paramlist(ndiv);
            std::vector<Base::Vector3d> pointatcurvelist(ndiv);
            std::vector<double> curvaturelist(ndiv);
            std::vector<Base::Vector3d> normallist(ndiv);

            double maxcurv = 0;
            double maxdisttocenterofmass = 0;

            for (int i = 0; i < ndiv; i++) {
                paramlist[i] = firstparam + i * step;
                pointatcurvelist[i] = geo->pointAtParameter(paramlist[i]);

                try {
                    curvaturelist[i] = geo->curvatureAt(paramlist[i]);
                }
                catch (Base::CADKernelError& e) {
                    // it is "just" a visualisation matter OCC could not calculate the curvature
                    // terminating here would mean that the other shapes would not be drawn.
                    // Solution: Report the issue and set dummy curvature to 0
                    e.reportException();
                    Base::Console().developerError(
                        "EditModeGeometryCoinConverter",
                        "Curvature graph for B-spline with GeoId=%d could not be calculated.\n",
                        geoid);  // TODO: Fix identification of curve.
                    curvaturelist[i] = 0;
                }

                if (curvaturelist[i] > maxcurv) {
                    maxcurv = curvaturelist[i];
                }

                double tempf = (pointatcurvelist[i] - midp).Length();

                if (tempf > maxdisttocenterofmass) {
                    maxdisttocenterofmass = tempf;
                }
            }

            double temprepscale = 0;
            if (maxcurv > 0) {
                temprepscale = (0.5 * maxdisttocenterofmass)
                    / maxcurv;  // just a factor to make a comb reasonably visible
            }

            if (temprepscale > combrepscale) {
                combrepscale = temprepscale;
            }
        }
    }
}

float EditModeGeometryCoinConverter::getBoundingBoxMaxMagnitude()
{
    return boundingBoxMaxMagnitude;
}

double EditModeGeometryCoinConverter::getCombRepresentationScale()
{
    return combrepscale;
}
