# Solar3D

## 1.Background
   Solar3D is a software application designed to interactively calculate solar irradiation and sky view factor at points on 3D surfaces. It is essentially a 3D extension of the GRASS GIS r.sun solar radiation model. According to the GRASS GIS documentation [1]:
   
   “r.sun - Solar irradiance and irradiation model. 
   Computes direct (beam), diffuse and reflected solar irradiation raster maps for given day, latitude, surface and atmospheric conditions. Solar parameters (e.g. sunrise, sunset times, declination, extraterrestrial irradiance, daylight length) are saved in the map history file. Alternatively, a local time can be specified to compute solar incidence angle and/or irradiance raster maps. The shadowing effect of the topography is optionally incorporated.”
   
   To calculate the solar irradiation over a certain time interval at a point on a 3D surface, Solar3D first derives the slope and aspect value from the surface normal, and then use a cube map to determine if the point is shaded at each time step for the entire duration. Instead of using the brute-force ray-casting which relies on ray-triangle intersection and therefore is computationally intensive, Solar3D generates a cube map-based panoramic view of the 3D scene at the point with sky and non-sky pixels encoded in different color values, and then determines if the point is shaded at each time step by looking up the intersected pixel in the corresponding cube map face (Figure 1). In this way, Solar3D can rapidly calculate daily to annual solar irradiation at arbitrary points in sufficiently small time-steps (smaller than one hour). However, a limitation with Solar3D is that it is designed specifically to rapidly calculate solar irradiation at discrete points and is not equipped with adequate performance to calculate for large areas where points are densely and uniformly distributed, and the main reason is that generating millions of cube maps is not computationally affordable.

 ## 2.Scene Preparation
   CAD models can be created in CAD software such as Autodesk 3ds Max, SketchUp and Blender and exported in a format that OpenSceneGraph supports [5]. OAP3Ds can be created using photo-based 3D reconstruction tools such as Esri Drone2Map and Skyline PhotoMesh and exported into OpenSceneGraph’s Paged LOD format. Building footprints-extruded 3D models need to be created in an osgEarth scene by apply 3D symbology to a feature layer.
   
   To create an osgEarth scene, find a template under “Solar3D/bin/tests/” that best suits your needs and then make a copy and modify in a text editor. Table 1 is a selected list of examples demonstrating how to start Solar3D with different types of 3D models (OAP3D, CAD, building footprint extrusions) with or without osgEarth scenes (global or local). 
   
Solar3D DEMOs:
(1) Solar3D/bin/Example_OAP3D.bat. Start with an OAP3D model.
(2) Solar3D/bin/Example_CAD.bat. Start with an CAD model.
(3) Solar3D/bin/Example_Boston_Global.bat. Start with a global osgEarth scene. This example shows how to extrude build footprints from a shapefile.
(4) Solar3D/bin/Example_Boston_Projected.bat. Start with a local (projected) osgEarth scene. This example shows how to extrude build footprints from a shapefile.
(5) Solar3D/bin/Example_Solar3D_Global.bat. Start with a global osgEarth scene. This example shows how to position CAD and OAP3D models in a global scene.
(6) Solar3D/bin/Example_Solar3D_Projected.bat. Start with a local osgEarth scene. This example shows how to position CAD and OAP3D models in a local scene.

 ## 3.Build From Source
**Required build tools:**
 CMake >= 3.15

**Required dependencies:**
The easiest way to obtain these dependencies is to install using vcpkg, a C++ package management tool for Windows, Linux and MacOS.
For Windows, a Visual Studio 2019 solution is available with all required dependencies included.

Dependency | vcpkg package
------------ | -------------
qt5-base >= 5.12 | vcpkg install qt5-base:x64-window
osgEarth >= 2.10 | vcpkg install osgearth:x64-windows
OpenSceneGraph >= 3.6.4 | vcpkg install osg:x64-windows
 
 ## References
1. GRASS GIS r.sun. Available online: https://grass.osgeo.org/grass78/manuals/r.sun.html
2. OpenSceneGraph. Available online: http://www.openscenegraph.org
3. osgEarth. Available online: http://osgearth.org
4. vcpkg. Available online: https://github.com/microsoft/vcpkg
5. OpenSceneGraph Plugins. http://www.openscenegraph.org/index.php/documentation/user-guides/61-osgplugins

 ## Demonstration Video
https://youtu.be/6zWNaCaH-RE

 ## Publication
Liang, J.; Gong, J.; Xie, X.; Sun, J. Solar3D: An Open-Source Tool for Estimating Solar Radiation in Urban Environments. ISPRS Int. J. Geo-Inf. 2020, 9, 524.
https://www.mdpi.com/2220-9964/9/9/524

## Contact:
Jianming Liang
jian9695@gmail.com

