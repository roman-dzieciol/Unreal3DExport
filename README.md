# Unreal3DExport
Unreal .3d Vertex Animation exporter for 3ds Max

Special thanks to andrei313 [http://www.andrei313.com].

## ABOUT
 
This 3ds Max plugin lets you export Unreal Engine vertex animations. There are already many utilities that let you do that but their workflow is unacceptable or they are unreliable.

This plugin simplifies the exporting procedure:
 * Menu File->Export to export all meshes and all animations.
 * Menu File->Export Selected to export selected meshes and their animations.
 * Mesh is rotated automatically so it faces the same direction as in Max.
 * Mesh doesn't have to fit inside a 127x127x127 box anymore.
 * Built-in mesh optimization, removes ugly vertex snapping from animations.
 * You can use the Note Track to store information about animation sequences   and notifications inside the max file. On export this information will be added to automatically generated import script.

## COMPATIBILITY

* Tested on 3ds Max 8 SP3, Windows XP SP2, UT2004 v3369
* Requires 3DXI library v2.2+
 * http://usa.autodesk.com/adsk/servlet/item?siteID=123112&id=7481484
 
## INSTALLATION

* shutdown 3ds Max
* Install latest 3DXI library
* Copy Unreal3DExport.dle to your 3ds Max plugins directory
 


## HOW TO: EXPORT

1. In 3ds Max go to menu "File -> Export"
 * Alternatively select some objects and go to menu "File -> Export Selected"
1. Choose "Unreal 3D" as filetype
1. Type filename without "_a" or "_d" postfixes, plugin will add them.
 * Alternatively double click on existing *_a.3d or *_d.3d file, they will be
     overwritten.
1. When exporting is finished a popup window with summary will be displayed.
1. Exporter generates nearly complete import script automatically, you'll find it in the same directory as .3d files. It contains required origin & scale adjustments.
   
   
      
## HOW TO: CANCEL / PAUSE EXPORTING

* Press the "esc" key to pause exporting and display cancel confirmation window.
   


## HOW TO: GENERATE ANIMATION INFO IN IMPORT SCRIPT

There are two kinds of information that can be written:
 * Animation Sequence (name, length, rate, group)
 * Animation Notification (time, function name)
 
Both require a Note Track attached to the World node:
 * Open the Dope Sheet window
 * Select the "World" node
 * In menu: Tracks -> Note Track -> Add
 * All notes added to this track will be scanned for special commands
 * Each command should be in it's own line
  
Animation Sequence command:
1. At the beginning of each animation, put one Note in the Note Track
1. In each note add following line: 
 * "a AnimName EndFrame FrameRate GroupName", where: 
  * "a" is a special marker, dont change it
  * "AnimName" is your animation name 
  * "EndFrame" is the number of frame where this animation ends
  * "FrameRate" is number of frames per second [OPTIONAL]
  * "GroupName" is name of animation group, this is rarely used [OPTIONAL]
 * Examples: 
   * "a Idle 60"
   * "a Idle 8 4"
   * "a Finger 20 10 Gesture"
      
Animation Notification command:
 * Notifications will be linked to last specified sequence
 * You can add a new note or reuse existing (put the command in new line)
 * Command syntax:
  * "n Function Time", where:
   * "n" is a special marker, dont change it
   * "Function" is the name of unrealscript function that will be called
   * "Time" is a number between 0.0 and 1.0, where 0.0 is first frame of the sequence and 1.0 is the last frame
 * Examples: 
   * "n PlayFootstep 0.40"
   * "n CreateBloodPool 0.90"
 
 
 
## HOW TO: ADD SPECIAL FLAGS TO POLYGONS

According to UDN the flags aren't used in UT2004. This functionality is  implemented just for the older games.

To set a flag on specified triangles, you need to assign a material with special name to them. The material must have "F=Number" in name, separated  with a space from any other text in material name. The "Number" should be a number between 0 and 255.

Flag number meanings according to 3ds2unr:
 ```
 0 = None
 2 = Translucent
 3 = Two Sided
 8 = Weapon Attachment
 ```
 Examples:
  * "F=3"
  * "1 - Standard F=2"



## HOW TO: TEXTURING

All materials ID's are preserved in the exported model. Material ID's with assigned material generate proper #exec commands in the importer script. 

Currently texture names aren't written to the exporter script, you'll have to enter them manually.
 
