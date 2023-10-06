# Windows

## Main Window

The main window is where any active experiment or event is displayed.

### Navigation

You can move the camera through the scene to view your simulation from any angle.
Use `wasd` to move forward, backward, left and right,
`q` and `e` to move up and down,
`r` and `f` to roll the camera.

While holding `control + left click` you can rotate the scene around the origin.
Note that this does not rotate the camera but rather the scene itself.
Using the `scroll wheel` you can move the camera forwards and backwards as well.

By pressing `Tab` you can toggle between the isometric and orthographic perspective.

By hitting `space` you can pause/resume playback of simulation.

### Scene alignment

Instead of using the mouse to rotate the scene
you may also use the *alignment* and *rotation* tools in the bar at the top of the window.
There you can align the view or the up direction of the scene along either of the three coordinate axes.
You can also rotate the scene by a fixed angle increment about any of three coordinate axes.
The angle step size can be adjusted in the `Step Size(°)` field.

### View

The appearance of the simulation results can be adjusted in the *view* dropdown menu in the top left corner
(see [View](7customization.md#view)).

## Tool Windows

The tool windows can be toggled in the `Tool Windows` dropdown menu in the top left corner.
When activated, they can be resized.

### Info Window

The *Info Window* has two modes, *Select and Tag* and *Measure*,
which can be selected from using the buttons in the top right corner.
Regardless of which mode is currently selected, some information stays the same across both modes:

| Element                      | Description                                                               |
|:-----------------------------|:--------------------------------------------------------------------------|
| *FPS counter*                | Displays the current frames per second for the program                    |
| *Movie Framerate*            | Slider to adjusts the framerate of the movie displayed                    |
| *Movie FrameIndex*           | Slider to set the current Frame Index; will move while a movie is running |
| *Loop Simulation*            | Checkbox to toggle, whether the movie loops                               |
| *Manual Movie Frame Control* | Checkbox to Pause/resume the movie                                        |
| *Cells X/Y/Z*                | Set the number of periodic cells in along the given axis                  |


#### Select and Tag

#### Measure

### Material Parameter Window

Opening the `Material Parameter Window` will allow you to adjust the appearance of your simulation results by component
via a collection of sliders. For more info, see [Material Parameters](7customization.md#material-parameters).

## Event Info Window

Once you have selected an [Event](5databases.md#events) the *Event Info Window* will appear on the bottom right.
There you can edit settings pertaining to the event currently displayed and view some data on the atoms participating
in the event as well as some details on the cylinder cropping.

An outline of the cylinder is rendered by default, but you may want to hide the cylinder border.
That can be done in the [View](7customization.md#view) dropdown.

The *Event Info Window* lets you edit the dimensions of the cropping cylinder:

| Element                 | Description                                                                                                            |
|:------------------------|:-----------------------------------------------------------------------------------------------------------------------|
| *Cylinder Culling*      | Checkbox to toggle, whether to hide anything outside the cropping cylinder                                             |
| *Use Surface Normal*    | Checkbox to set the cylinder's main axis along the catalyst's surface normal vector (or their average, if multiple)    |
| *Use Connection Normal* | Checkbox to set the cylinder's main axis along the chemical's connection normal vector (or their average, if multiple) |
| *Leave Event Mode*      | Button to leave *event mode*                                                                                           |
| *Cylinder Length*       | Slider to set the cylinder height.                                                                                     |
| *Cylinder Radius*       | Slider to set the cylinder base radius                                                                                 |

Displayed beneath is a list of the atoms participating in the event and the internal values for the cylinder dimensions.

## User Preferences Window

Lastly there is the *User Preferences* Window, which can be accessed by clicking on `File > User Preferences`
in the top left corner. For information on the customization options within, see