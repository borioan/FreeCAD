# SPDX-License-Identifier: LGPL-2.1-or-later

# ***************************************************************************
# *                                                                         *
# *   Copyright (c) 2016 Yorik van Havre <yorik@uncreated.net>              *
# *                                                                         *
# *   This file is part of FreeCAD.                                         *
# *                                                                         *
# *   FreeCAD is free software: you can redistribute it and/or modify it    *
# *   under the terms of the GNU Lesser General Public License as           *
# *   published by the Free Software Foundation, either version 2.1 of the  *
# *   License, or (at your option) any later version.                       *
# *                                                                         *
# *   FreeCAD is distributed in the hope that it will be useful, but        *
# *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
# *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU      *
# *   Lesser General Public License for more details.                       *
# *                                                                         *
# *   You should have received a copy of the GNU Lesser General Public      *
# *   License along with FreeCAD. If not, see                               *
# *   <https://www.gnu.org/licenses/>.                                      *
# *                                                                         *
# ***************************************************************************

__title__  = "Arch Pipe tools"
__author__ = "Yorik van Havre"
__url__    = "https://www.freecad.org"

## @package ArchPipe
#  \ingroup ARCH
#  \brief The Pipe object and tools
#
#  This module provides tools to build Pipe and Pipe connector objects.
#  Pipes are tubular objects extruded along a base line.

import FreeCAD
import ArchComponent
import ArchIFC

from draftutils import params

if FreeCAD.GuiUp:
    from PySide.QtCore import QT_TRANSLATE_NOOP
    import FreeCADGui
    import Arch_rc
    from draftutils.translate import translate
else:
    # \cond
    def translate(ctxt,txt):
        return txt
    def QT_TRANSLATE_NOOP(ctxt,txt):
        return txt
    # \endcond


class _ArchPipe(ArchComponent.Component):


    "the Arch Pipe object"

    def __init__(self,obj):

        ArchComponent.Component.__init__(self,obj)
        self.Type = "Pipe"
        self.setProperties(obj)
        # IfcPipeSegment is new in IFC4
        from ArchIFC import IfcTypes
        if "Pipe Segment" in IfcTypes:
            obj.IfcType = "Pipe Segment"
        else:
            # IFC2x3 does not know a Pipe Segment
            obj.IfcType = "Building Element Proxy"

    def setProperties(self,obj):

        pl = obj.PropertiesList
        if not "Diameter" in pl:
            obj.addProperty("App::PropertyLength", "Diameter",     "Pipe", QT_TRANSLATE_NOOP("App::Property","The diameter of this pipe, if not based on a profile"), locked=True)
        if not "Width" in pl:
            obj.addProperty("App::PropertyLength", "Width",        "Pipe", QT_TRANSLATE_NOOP("App::Property","The width of this pipe, if not based on a profile"), locked=True)
            obj.setPropertyStatus("Width", "Hidden")
        if not "Height" in pl:
            obj.addProperty("App::PropertyLength", "Height",        "Pipe", QT_TRANSLATE_NOOP("App::Property","The height of this pipe, if not based on a profile"), locked=True)
            obj.setPropertyStatus("Height", "Hidden")
        if not "Length" in pl:
            obj.addProperty("App::PropertyLength", "Length",       "Pipe", QT_TRANSLATE_NOOP("App::Property","The length of this pipe, if not based on an edge"), locked=True)
        if not "Profile" in pl:
            obj.addProperty("App::PropertyLink",   "Profile",      "Pipe", QT_TRANSLATE_NOOP("App::Property","An optional closed profile to base this pipe on"), locked=True)
        if not "OffsetStart" in pl:
            obj.addProperty("App::PropertyLength", "OffsetStart",  "Pipe", QT_TRANSLATE_NOOP("App::Property","Offset from the start point"), locked=True)
        if not "OffsetEnd" in pl:
            obj.addProperty("App::PropertyLength", "OffsetEnd",    "Pipe", QT_TRANSLATE_NOOP("App::Property","Offset from the end point"), locked=True)
        if not "WallThickness" in pl:
            obj.addProperty("App::PropertyLength", "WallThickness","Pipe", QT_TRANSLATE_NOOP("App::Property","The wall thickness of this pipe, if not based on a profile"), locked=True)
        if not "ProfileType" in pl:
            obj.addProperty("App::PropertyEnumeration", "ProfileType", "Pipe", QT_TRANSLATE_NOOP("App::Property","If not based on a profile, this controls the profile of this pipe"), locked=True)
            obj.ProfileType = ["Circle", "Square", "Rectangle"]

    def onDocumentRestored(self,obj):

        ArchComponent.Component.onDocumentRestored(self,obj)
        self.setProperties(obj)
        if obj.ProfileType == "Rectangle" and FreeCAD.ActiveDocument.getProgramVersion().split()[0] <= "1.1R42474":
            # in older versions Height and Width are inverted
            obj.Height, obj.Width = obj.Width, obj.Height
            FreeCAD.ActiveDocument.recompute()
            FreeCAD.Console.PrintWarning(
                "v1.1, "
                + obj.Label
                + ", "
                + translate("Arch", "corrected 'Height' and 'Width' properties")
                + "\n"
            )

    def loads(self,state):

        self.Type = "Pipe"

    def onChanged(self, obj, prop):
        if prop == "IfcType":
            root = ArchIFC.IfcProduct()
            root.setupIfcAttributes(obj)
            root.setupIfcComplexAttributes(obj)
        elif prop == "ProfileType":
            if obj.ProfileType == "Square":
                obj.setPropertyStatus("Height", "Hidden")
                obj.setPropertyStatus("Diameter", "Hidden")
                obj.setPropertyStatus("Width", "-Hidden")
            elif obj.ProfileType == "Rectangle":
                obj.setPropertyStatus("Height", "-Hidden")
                obj.setPropertyStatus("Diameter", "Hidden")
                obj.setPropertyStatus("Width", "-Hidden")
            else:
                obj.setPropertyStatus("Height", "Hidden")
                obj.setPropertyStatus("Diameter", "-Hidden")
                obj.setPropertyStatus("Width", "Hidden")

    def execute(self,obj):

        import math
        import Part
        import DraftGeomUtils
        if self.clone(obj):
            return
        pl = obj.Placement
        w = self.getWire(obj)
        if not w:
            FreeCAD.Console.PrintError(translate("Arch","Unable to build the base path")+"\n")
            return
        if obj.OffsetStart.Value:
            e = w.Edges[0]
            v = e.Vertexes[-1].Point.sub(e.Vertexes[0].Point).normalize()
            v.multiply(obj.OffsetStart.Value)
            e = Part.LineSegment(e.Vertexes[0].Point.add(v),e.Vertexes[-1].Point).toShape()
            w = Part.Wire([e]+w.Edges[1:])
        if obj.OffsetEnd.Value:
            e = w.Edges[-1]
            v = e.Vertexes[0].Point.sub(e.Vertexes[-1].Point).normalize()
            v.multiply(obj.OffsetEnd.Value)
            e = Part.LineSegment(e.Vertexes[-1].Point.add(v),e.Vertexes[0].Point).toShape()
            w = Part.Wire(w.Edges[:-1]+[e])
        p = self.getProfile(obj)
        if not p:
            FreeCAD.Console.PrintError(translate("Arch","Unable to build the profile")+"\n")
            return
        # move and rotate the profile to the first point
        if hasattr(p,"CenterOfMass"):
            c = p.CenterOfMass
        else:
            c = p.BoundBox.Center
        delta = w.Vertexes[0].Point-c
        p.translate(delta)
        import Draft
        if Draft.getType(obj.Base) == "BezCurve":
            v1 = obj.Base.Placement.multVec(obj.Base.Points[1])-w.Vertexes[0].Point
        else:
            v1 = w.Vertexes[1].Point-w.Vertexes[0].Point
        #v2 = DraftGeomUtils.getNormal(p)
        #rot = FreeCAD.Rotation(v2,v1)
        # rotate keeping up vector
        if v1.getAngle(FreeCAD.Vector(0,0,1)) > 0.01:
            up = FreeCAD.Vector(0,0,1)
        else:
            up = FreeCAD.Vector(0,1,0)
        v1y = up.cross(v1)
        v1x = v1.cross(v1y)
        rot = FreeCAD.Rotation(v1x,v1y,v1,"ZYX")
        p.rotate(w.Vertexes[0].Point,rot.Axis,math.degrees(rot.Angle))
        p.rotate(w.Vertexes[0].Point,v1,90)
        shapes = []
        try:
            if p.Faces:
                for f in p.Faces:
                    sh = w.makePipeShell([f.OuterWire],True,False,2)
                    for shw in f.Wires:
                        if shw.hashCode() != f.OuterWire.hashCode():
                            sh2 = w.makePipeShell([shw],True,False,2)
                            sh = sh.cut(sh2)
                    shapes.append(sh)
            elif p.Wires:
                for pw in p.Wires:
                    sh = w.makePipeShell([pw],True,False,2)
                    shapes.append(sh)
        except Exception:
            FreeCAD.Console.PrintError(translate("Arch","Unable to build the pipe")+"\n")
        else:
            if len(shapes) == 0:
                return
            elif len(shapes) == 1:
                sh = shapes[0]
            else:
                sh = Part.makeCompound(shapes)
            obj.Shape = self.processSubShapes(obj,sh,pl)
            if obj.Base:
                obj.Length = w.Length
            else:
                obj.Placement = pl

    def getWire(self,obj):

        import Part
        if obj.Base:
            if not hasattr(obj.Base,'Shape'):
                FreeCAD.Console.PrintError(translate("Arch","The base object is not a Part")+"\n")
                return
            if len(obj.Base.Shape.Wires) != 1:
                FreeCAD.Console.PrintError(translate("Arch","Too many wires in the base shape")+"\n")
                return
            if obj.Base.Shape.Wires[0].isClosed():
                FreeCAD.Console.PrintError(translate("Arch","The base wire is closed")+"\n")
                return
            w = obj.Base.Shape.Wires[0]
        else:
            if obj.Length.Value == 0:
                return
            w = Part.Wire([Part.LineSegment(FreeCAD.Vector(0,0,0),FreeCAD.Vector(0,0,obj.Length.Value)).toShape()])
        return w

    def getProfile(self,obj):

        import Part
        if obj.Profile:
            if not obj.Profile.getLinkedObject().isDerivedFrom("Part::Part2DObject"):
                FreeCAD.Console.PrintError(translate("Arch","The profile is not a 2D Part")+"\n")
                return
            if not obj.Profile.Shape.Wires[0].isClosed():
                FreeCAD.Console.PrintError(translate("Arch","The profile is not closed")+"\n")
                return
            p = obj.Profile.Shape.Wires[0]
        else:
            if obj.ProfileType == "Square":
                if obj.Width.Value == 0:
                    return
                p = Part.makePlane(obj.Width.Value, obj.Width.Value,FreeCAD.Vector(-obj.Width.Value/2,-obj.Width.Value/2,0))
                if obj.WallThickness.Value and (obj.WallThickness.Value < obj.Width.Value/2):
                    p2 = Part.makePlane(obj.Width.Value-obj.WallThickness.Value*2,obj.Width.Value-obj.WallThickness.Value*2,FreeCAD.Vector(obj.WallThickness.Value-obj.Width.Value/2,obj.WallThickness.Value-obj.Width.Value/2,0))
                    p = p.cut(p2)
            elif obj.ProfileType == "Rectangle":
                if obj.Width.Value == 0:
                    return
                if obj.Height.Value == 0:
                    return
                p = Part.makePlane(obj.Width.Value, obj.Height.Value,FreeCAD.Vector(-obj.Width.Value/2,-obj.Height.Value/2,0))
                if obj.WallThickness.Value and (obj.WallThickness.Value < obj.Height.Value/2) and (obj.WallThickness.Value < obj.Width.Value/2):
                    p2 = Part.makePlane(obj.Width.Value-obj.WallThickness.Value*2,obj.Height.Value-obj.WallThickness.Value*2,FreeCAD.Vector(obj.WallThickness.Value-obj.Width.Value/2,obj.WallThickness.Value-obj.Height.Value/2,0))
                    p = p.cut(p2)
            else:
                if obj.Diameter.Value == 0:
                    return
                p = Part.Wire([Part.Circle(FreeCAD.Vector(0,0,0),FreeCAD.Vector(0,0,1),obj.Diameter.Value/2).toShape()])
                if obj.WallThickness.Value and (obj.WallThickness.Value < obj.Diameter.Value/2):
                    p2 = Part.Wire([Part.Circle(FreeCAD.Vector(0,0,0),FreeCAD.Vector(0,0,1),(obj.Diameter.Value/2-obj.WallThickness.Value)).toShape()])
                    p = Part.Face(p)
                    p2 = Part.Face(p2)
                    p = p.cut(p2)
        return p


class _ViewProviderPipe(ArchComponent.ViewProviderComponent):


    "A View Provider for the Pipe object"

    def __init__(self,vobj):

        ArchComponent.ViewProviderComponent.__init__(self,vobj)

    def getIcon(self):

        import Arch_rc
        return ":/icons/Arch_Pipe_Tree.svg"


class _ArchPipeConnector(ArchComponent.Component):


    "the Arch Pipe Connector object"

    def __init__(self,obj):

        ArchComponent.Component.__init__(self,obj)
        self.setProperties(obj)
        obj.IfcType = "Pipe Fitting"

    def setProperties(self,obj):

        pl = obj.PropertiesList
        if not "Radius" in pl:
            obj.addProperty("App::PropertyLength",      "Radius",        "PipeConnector", QT_TRANSLATE_NOOP("App::Property","The curvature radius of this connector"), locked=True)
        if not "Pipes" in pl:
            obj.addProperty("App::PropertyLinkList",    "Pipes",         "PipeConnector", QT_TRANSLATE_NOOP("App::Property","The pipes linked by this connector"), locked=True)
        if not "ConnectorType" in pl:
            obj.addProperty("App::PropertyEnumeration", "ConnectorType", "PipeConnector", QT_TRANSLATE_NOOP("App::Property","The type of this connector"), locked=True)
            obj.ConnectorType = ["Corner","Tee"]
            obj.setEditorMode("ConnectorType",1)
        self.Type = "PipeConnector"

    def onDocumentRestored(self,obj):

        ArchComponent.Component.onDocumentRestored(self,obj)
        self.setProperties(obj)

    def execute(self,obj):

        if self.clone(obj):
            return

        tol = 1 # tolerance for alignment. This is only visual, we can keep it low...
        ptol = 0.001 # tolerance for coincident points

        import math
        import Part
        import DraftGeomUtils
        import ArchCommands
        if len(obj.Pipes) < 2:
            return
        if len(obj.Pipes) > 3:
            FreeCAD.Console.PrintWarning(translate("Arch","Only the 3 first wires will be connected")+"\n")
        if obj.Radius.Value == 0:
            return
        wires = []
        order = []
        for o in obj.Pipes:
            wires.append(o.Proxy.getWire(o))
        if wires[0].Vertexes[0].Point.sub(wires[1].Vertexes[0].Point).Length <= ptol:
            order = ["start","start"]
            point = wires[0].Vertexes[0].Point
        elif wires[0].Vertexes[0].Point.sub(wires[1].Vertexes[-1].Point).Length <= ptol:
            order = ["start","end"]
            point = wires[0].Vertexes[0].Point
        elif wires[0].Vertexes[-1].Point.sub(wires[1].Vertexes[-1].Point).Length <= ptol:
            order = ["end","end"]
            point = wires[0].Vertexes[-1].Point
        elif wires[0].Vertexes[-1].Point.sub(wires[1].Vertexes[0].Point).Length <= ptol:
            order = ["end","start"]
            point = wires[0].Vertexes[-1].Point
        else:
            FreeCAD.Console.PrintError(translate("Arch","Common vertex not found")+"\n")
            return
        if order[0] == "start":
            v1 = wires[0].Vertexes[1].Point.sub(wires[0].Vertexes[0].Point).normalize()
        else:
            v1 = wires[0].Vertexes[-2].Point.sub(wires[0].Vertexes[-1].Point).normalize()
        if order[1] == "start":
            v2 = wires[1].Vertexes[1].Point.sub(wires[1].Vertexes[0].Point).normalize()
        else:
            v2 = wires[1].Vertexes[-2].Point.sub(wires[1].Vertexes[-1].Point).normalize()
        p = obj.Pipes[0].Proxy.getProfile(obj.Pipes[0])
        if not p:
            return
        # If the pipe has a non-zero WallThickness p is a shape instead of a wire:
        if p.ShapeType != "Wire":
            p = p.Wires
        p = Part.Face(p)
        if len(obj.Pipes) == 2:
            if obj.ConnectorType != "Corner":
                obj.ConnectorType = "Corner"
            if round(v1.getAngle(v2),tol) in [0,round(math.pi,tol)]:
                FreeCAD.Console.PrintError(translate("Arch","Pipes are already aligned")+"\n")
                return
            normal = v2.cross(v1)
            offset = math.tan(math.pi/2-v1.getAngle(v2)/2)*obj.Radius.Value
            v1.multiply(offset)
            v2.multiply(offset)
            self.setOffset(obj.Pipes[0],order[0],offset)
            self.setOffset(obj.Pipes[1],order[1],offset)
            # find center
            perp = v1.cross(normal).normalize()
            perp.multiply(obj.Radius.Value)
            center = point.add(v1).add(perp)
            # move and rotate the profile to the first point
            delta = point.add(v1)-p.CenterOfMass
            p.translate(delta)
            #vp = DraftGeomUtils.getNormal(p)
            #rot = FreeCAD.Rotation(vp,v1)
            # rotate keeping up vector
            if v1.getAngle(FreeCAD.Vector(0,0,1)) > 0.01:
                up = FreeCAD.Vector(0,0,1)
            else:
                up = FreeCAD.Vector(0,1,0)
            v1y = up.cross(v1)
            v1x = v1.cross(v1y)
            rot = FreeCAD.Rotation(v1x,v1y,v1,"ZYX")
            p.rotate(p.CenterOfMass,rot.Axis,math.degrees(rot.Angle))
            p.rotate(p.CenterOfMass,v1,90)
            try:
                sh = p.revolve(center,normal,math.degrees(math.pi-v1.getAngle(v2)))
            except:
                FreeCAD.Console.PrintError(translate("Arch","Unable to revolve this connector")+"\n")
                return
            #sh = Part.makeCompound([sh]+[Part.Vertex(point),Part.Vertex(point.add(v1)),Part.Vertex(center),Part.Vertex(point.add(v2))])
        else:
            if obj.ConnectorType != "Tee":
                obj.ConnectorType = "Tee"
            if wires[2].Vertexes[0].Point == point:
                order.append("start")
            elif wires[0].Vertexes[-1].Point == point:
                order.append("end")
            else:
                FreeCAD.Console.PrintError(translate("Arch","Common vertex not found")+"\n")
            if order[2] == "start":
                v3 = wires[2].Vertexes[1].Point.sub(wires[2].Vertexes[0].Point).normalize()
            else:
                v3 = wires[2].Vertexes[-2].Point.sub(wires[2].Vertexes[-1].Point).normalize()
            if round(v1.getAngle(v2),tol) in [0,round(math.pi,tol)]:
                pair = [v1,v2,v3]
            elif round(v1.getAngle(v3),tol) in [0,round(math.pi,tol)]:
                pair = [v1,v3,v2]
            elif round(v2.getAngle(v3),tol) in [0,round(math.pi,tol)]:
                pair = [v2,v3,v1]
            else:
                FreeCAD.Console.PrintError(translate("Arch","At least 2 pipes must align")+"\n")
                return
            offset = obj.Radius.Value
            v1.multiply(offset)
            v2.multiply(offset)
            v3.multiply(offset)
            self.setOffset(obj.Pipes[0],order[0],offset)
            self.setOffset(obj.Pipes[1],order[1],offset)
            self.setOffset(obj.Pipes[2],order[2],offset)
            normal = pair[0].cross(pair[2])
            # move and rotate the profile to the first point
            delta = point.add(pair[0])-p.CenterOfMass
            p.translate(delta)
            vp = DraftGeomUtils.getNormal(p)
            rot = FreeCAD.Rotation(vp,pair[0])
            p.rotate(p.CenterOfMass,rot.Axis,math.degrees(rot.Angle))
            t1 = p.extrude(pair[1].multiply(2))
            # move and rotate the profile to the second point
            delta = point.add(pair[2])-p.CenterOfMass
            p.translate(delta)
            vp = DraftGeomUtils.getNormal(p)
            rot = FreeCAD.Rotation(vp,pair[2])
            p.rotate(p.CenterOfMass,rot.Axis,math.degrees(rot.Angle))
            t2 = p.extrude(pair[2].negative().multiply(2))
            # create a cut plane
            cp = Part.makePolygon([point,point.add(pair[0]),point.add(normal),point])
            cp = Part.Face(cp)
            if cp.normalAt(0,0).getAngle(pair[2]) < math.pi/2:
                cp.reverse()
            cf, cv, invcv = ArchCommands.getCutVolume(cp,t2)
            t2 = t2.cut(cv)
            sh = t1.fuse(t2)
        obj.Shape = sh

    def setOffset(self,pipe,pos,offset):

        if pos == "start":
            if pipe.OffsetStart != offset:
                pipe.OffsetStart = offset
                pipe.Proxy.execute(pipe)
        else:
            if pipe.OffsetEnd != offset:
                pipe.OffsetEnd = offset
                pipe.Proxy.execute(pipe)
