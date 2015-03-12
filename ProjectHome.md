# what is it? #

.kkapture is a small tool that produces video+audio captures of fullscreen apps (usually demos). unlike [fraps](http://www.fraps.com/), it does not run in realtime; instead, it makes the demos run at a given, fixed framerate you can specify beforehand. in other words, .kkapture can make 60fps video captures of any demo your computer can run, even if each frame takes several seconds to render.
# why would someone want this? #

for windows demos, it is usually not much work to add a video writer to your program. however, that has to be done by the authors; the nice part about .kkapture is that it is sufficiently general to work with a wide variety of demos, automatically. if you do demos yourself and .kkapture is able to handle your demo without problems, well, you've just saved yourself the work of coding a video writer yourself.

the main application area is when you want to make several quality video captures of different demos in a short period of time, without having to contact each of the coders. production of demo dvds or pre-cut demoshows would be a prime example.
how did it come to be?

well, rather simple story; i was rather annoyed by the generally so-so video quality of the 2004 [scene.org](http://scene.org/) awards, mostly caused by repeated frame-rate changes (first fraps which captures at whatever rate the demo runs, then video encoding, then playback on a computer with whatever refresh rate it's using, then conversion by the beamer). the fixed framerate capturing should be able to get rid of most of these problems, by simply avoiding the necessity of framerate conversions altogether.
# how does it work? #

if you want the gory details, just look at the provided source code. here's a short executive summary: .kkapture works by hooking certain graphics api functions to capture each frame just as it is presented on screen. all popular ways of getting time into the program are wrapped as well - timeGetTime, QueryPerformanceCounter, you name it. this is necessary so .kkapture can make the program think it runs at a fixed framerate (whatever you specified). the most tricky part is audio, because it doubles as both capture signal and sync source. .kkapture provides a custom (and very dumb) DirectSound implementation that emulates an actual soundcard again updating with your exact specified framerate. waveout support is present too, though it is untested at the moment, so beware.
# how to use? #

well, download it, start kkapture.exe, specify demo/video file name and framerate, hit "kkapture!", select your codec of choice and that should be it :). however, you have to execute the demos in fullscreen mode; i currently use mode switches to determine the size of the image to capture. this is not a technical necessity, but it was the simplest way to get that information at the time, and .kkapture is still in a very early stage (as the version number might suggest ;).

about "skip silence" and "make sleeps last one frame": both these options fix problems for certain demos/intros and cause problems with others. the default settings are what works best most of the time, but if you're experiencing problems, try setting these switches to other settings.
where can i get it?
# known issues #

  * lots of incompatibilities with demos that don't make sense to list here.
  * the source code could use some cleanup.
# credits #

refer to the ["CONTRIBUTORS.txt"](http://code.google.com/p/kkapture/source/browse/trunk/CONTRIBUTORS.txt).
# contact #

drop a mail: ryg(a)theprodukkt,com