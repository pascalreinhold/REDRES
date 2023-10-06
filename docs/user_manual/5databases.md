# Importing and navigating Experiments

## Importing Databases

Once you have opened the REDRES, if you haven't yet, you can load a database by clicking on `File > Load Database`.
Next, navigate to your database file and select it. 
Similarly, a database can be unloaded by clicking on `File > Unload Database`.

## Experiments

After loading a database, the experiment list will appear in the sidebar on the left,
where you can select one of the experiments by clicking the `load` button.
The currently selected experiment will be marked as `active`.
Each *experiment* may be expanded to view it's associated parameters,
those being divided into the *system* parameters and the *experiment settings*. Both are list that can be expanded to
view further details. By hovering over the entries, you can get a tooltip for each one.

## Events

The last list associated with an experiment is the *event* list. There, events are sorted by their *eventID*.
In the rightmost column there is a `jump` button which will take you to the desired event, pulling up a cropped section
of the simulation in the [Main Window](6windows.md#main-window),
centered on the average position of all atoms participating in the event.
The currently selected event will be marked as `active` in the event list.
You can exit the event mode by clicking on the `Leave Event Mode` button in the [Event Info Window](6windows.md#event-info-window),
where you'll also be able to find more event-related information and settings.