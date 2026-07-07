<!-- vim:set filetype=markdown: -->
# How do I prevent save files from getting corrupted?
This is unfortunately an annoying thing with modern versions of android. Normally, the app is meant to save and quit when the app is closed, but android may just kill the process in the middle of this saving. This will produce a truncated save file that cannot be reloaded. To prevent this, every time a valid save file is loaded, a backup is made that you can later restore, but that is not ideal since you would still lose all the data from _the last session_. The best way to circumvent this (or at least make it highly unlikely) is to **go to Settings -> Apps -> Nethack 5.0 -> Battery Optimizations and make it's background usage unrestricted**. This app does basically no background processing so I don't know why this is necessary, but @DMC4EVERUCCI said this fixed the problem for them. For more information look at [this thread](https://github.com/JodiJodington/NetHack-Android/issues/19). If you use the old gurrhack version of nethack-android on a modern phone, do this for that app as well since it suffers from the same bug _and_ doesn't make any attempt to backup good save files in case it gets corrupted on the next save attempt.
If you're below android 14, I don't think you have to do this, but if you see the option maybe do it anyway just to be sure.

# What apk file do I get?
It depends on your hardware. Most people will want to get the arm64-v8a one, but if your system is quite old you may need the armeabi one. As far as I know, phones never used x86 or x86\_64 so that would be for weird android-based devices like old chromebooks and dev consoles.

# Tracking updates automatically
you can use something like [Obtanium](https://github.com/ImranR98/Obtainium) for this. You can just point it at this repo and it will take care of everything else.
Note that I do not plan to put this on any kind of app store because it requires quite a lot of maintenance on my part (I am just one person) and there just really is no benefit that I can see.

# Verification Info:
You can use this to verify APK integrity manually with apksigner or with a tool like [AppVerifier](https://github.com/soupslurpr/AppVerifier).
`com.tbd.NetHack5`
`A3:93:35:69:6E:26:E5:DD:6C:1B:2D:08:C0:1C:78:A5:71:B9:2F:08:A3:63:5A:A8:9C:21:D9:0D:3A:A0:B8:EF`
