#!/bin/bash

#TODO(bjorn): to support apps below 21 only 32-bit binarys can be created.
# so even when i compile for 14 i should compile 21 64 bit binaries so 
# that more recent hardware can use the app effectively.

#NOTE(bjorn): These need to be defined by the developer.
minSdkVersion=21
targetSdkVersion=26
studio_name=kuma
app_name=game_engine

android_sdk=~/Library/Android/sdk
jdk="/Applications/Android Studio.app/Contents/jre/jdk/Contents/Home"

#NOTE(bjorn): All of the script below should automatically figure stuff out 
# from this point onwards.

ABI=arm64-v8a
machine_prefix=aarch64
ABI_simple=arm64

result_path=../build/android/puzzle_game/$ABI_simple

mkdir -p $result_path/lib/$ABI

cc=$android_sdk/ndk-bundle/toolchains/\
$machine_prefix-linux-android-4.9/prebuilt/\
darwin-x86_64/bin/$machine_prefix-linux-android-gcc

pushd $result_path

#NOTE(bjorn): Only run once.
#ln -s ../../../../code/*.h ./
#ln -s ../../../../code/*.cpp ./
#ln -s $android_sdk/ndk-bundle/platforms/\
#android-$minSdkVersion/arch-$ABI_simple/usr/lib/crtbegin_so.o
#ln -s $android_sdk/ndk-bundle/platforms/\
#android-$minSdkVersion/arch-$ABI_simple/usr/lib/crtend_so.o
#
#ln -s $latest_build_tools/aapt
#ln -s $latest_build_tools/dx
#ln -s $latest_build_tools/zipalign
#ln -s "$jdk/bin/javac"
#ln -s "$jdk/bin/jarsigner"
#ln -s $android_sdk/platform-tools/adb

$cc -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 -Wno-attributes \
android_game.cpp \
-x c++ $android_sdk/ndk-bundle/sources/android/native_app_glue/\
android_native_app_glue.c \
-I$android_sdk/ndk-bundle/sysroot/usr/include \
-I$android_sdk/ndk-bundle/sysroot/usr/include/$machine_prefix-linux-android \
-I$android_sdk/ndk-bundle/sources/android/native_app_glue \
-fPIC -shared -g -u ANativeActivity \
-L$android_sdk/ndk-bundle/platforms/android-$minSdkVersion/arch-$ABI_simple/usr/lib \
-lGLESv1_CM -lEGL -landroid -llog \
-o lib/$ABI/lib$app_name.so

echo "<?xml version=\"1.0\" encoding=\"utf-8\"?>
<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"
          package=\"com.$studio_name.$app_name\"
          android:versionCode=\"1\"
          android:versionName=\"1.0\">
	<uses-sdk android:minSdkVersion=\"$minSdkVersion\" 
            android:targetSdkVersion=\"$targetSdkVersion\" />
  <application android:icon=\"@mipmap/ic_launcher\"
                android:label=\"@string/app_name\"
                android:debuggable=\"true\">
    <activity android:name=\"android.app.NativeActivity\"
              android:label=\"@string/app_name\"
              android:configChanges=\"orientation|keyboardHidden\">
      <meta-data android:name=\"android.app.lib_name\"
                 android:value=\"$app_name\" />
      <intent-filter>
        <action android:name=\"android.intent.action.MAIN\" />
        <category android:name=\"android.intent.category.LAUNCHER\" />
      </intent-filter>
    </activity>
  </application>
</manifest>" > AndroidManifest.xml

# rem TODO(bjorn): Automatically find the latest build tools.
latest_build_tools=$android_sdk/build-tools/27.0.3

mkdir -p bin
mkdir -p gen

./aapt package -f -m -J gen -M AndroidManifest.xml \
-S ../../../../data/res \
-I $android_sdk/platforms/android-$minSdkVersion/android.jar
 
./javac -classpath $android_sdk/platforms/android-$minSdkVersion/android.jar \
-sourcepath . \
-d bin \
-bootclasspath "$jdk/jre/lib/rt.jar" -target 1.7 -source 1.7 \
gen/com/$studio_name/$app_name/R.java

export JAVA_HOME="$jdk"
./dx --dex --output=classes.dex bin 

./aapt package -f -M AndroidManifest.xml \
-S ../../../../data/res \
-I $android_sdk/platforms/android-$minSdkVersion/android.jar \
-F $app_name.apk.unaligned

./aapt add $app_name.apk.unaligned classes.dex > /dev/null

# rem aapt uses the path as-is for deciding where in the apk the files are
# rem located, which is a problem when you are on windows and
# rem use forward slashes without thinking...
# rem https://ibotpeaches.github.io/Apktool/documentation/ debug tool shoutout!!
# set gdbserver_path=%android_ndk%\prebuilt\android-%ABI_simple%\gdbserver
# copy %gdbserver_path%\gdbserver lib\%ABI%\. > nul
# %aapt% add %app_name%.apk.unaligned lib/%ABI%/gdbserver > nul
# TODO(bjorn): Automatically add all files in lib.
./aapt add $app_name.apk.unaligned lib/$ABI/lib$app_name.so > /dev/null

./jarsigner -keystore ~/.android/debug.keystore \
-storepass android \
-tsa http://sha256timestamp.ws.symantec.com/sha256/timestamp \
$app_name.apk.unaligned androiddebugkey > /dev/null

# create a release version with your keys
# jarsigner -keystore /path/to/your/release/keystore -storepass 'yourkeystorepassword' \
# ${_APK_BASENAME}.apk.unaligned yourkeystorename 

./zipalign -f 4 $app_name.apk.unaligned $app_name-debug.apk

./adb install -r $app_name-debug.apk

popd
