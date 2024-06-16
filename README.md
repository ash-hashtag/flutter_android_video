# flutter_android_video_test

Tried to make flutter work with android ndk media codecs

I have no idea about codecs

I just copy pasted from https://github.com/android/ndk-samples/blob/master/native-codec/app/src/main/cpp/native-codec-jni.cpp and patched it enough to work it out

Can't get audio to work tho :/
I didn't want to go through the rabbit hole to make it work

I wanted to interact with android ndk from dart directly, but that didn't look like possible, it relys on java objects

Put up the video_player(default exoplayer) and flutter_vlc_player for reference, to compare


## Getting Started

Get some video files onto the emulator or the device you are going to test with
Run the application, pick some video files, and then check how each player handles the videos

