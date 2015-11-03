Multitap Delay AU notes (August, 2003)
--------------------------------------

The Multitap's custom view is now built in a separate binary, and weak-linked from the AU.  The AU has a hard link to the ViewComponent's binary, and you'll need to install the AU & ViewComponent appropriately (or adjust the hardlink) to get the custom view to appear correctly in a host app.

Install the built view component (MultitapAUView) in MultitapAU.component/Contents/MacOS

Then copy that entire component into /Library/Audio/Plug-Ins/Components/

Then run your host app, and you should see the AU, and get its custom view