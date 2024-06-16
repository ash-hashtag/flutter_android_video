package com.example.flutter_android_video_test

import android.content.Context
import android.graphics.SurfaceTexture
import android.view.Surface
import android.view.SurfaceHolder
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel
import io.flutter.view.TextureRegistry.SurfaceTextureEntry

external fun shutdown(flNativeVideoPointer: Long);
external fun newFlNativeVideoFromfile(filePath: String, surface: Surface): Long;
external fun setPlayingStreamingMediaPlayer(isPlaying: Boolean, videoPointer: Long);

data class FlNativeVideo(
    val entry: SurfaceTextureEntry,
    val surface: Surface,
    val nativePointer: Long,
);

class MainActivity : FlutterActivity() {
    private val surfaces = mutableListOf<Pair<SurfaceTextureEntry, Surface>>()
    private val resourceHolder = mutableMapOf<Long, FlNativeVideo>()

    init {
        System.loadLibrary("native-codec-jni")
    }

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)

        MethodChannel(
            flutterEngine.dartExecutor.binaryMessenger,
            "fl_video"
        ).setMethodCallHandler { call, result ->
       
            when (call.method) {

                "flNativeVideoFromfile" -> {
                    val args = call.arguments<Map<String, Any>>()!!
                    val filePath = args["filePath"] as String;
                    // val fileSize = args["fileSize"] as Int;

                    val surfaceTexture = flutterEngine.renderer.createSurfaceTexture()
                    val surface = Surface(surfaceTexture.surfaceTexture())

                    val pointer = newFlNativeVideoFromfile(filePath, surface);
                    if (pointer == 0L) {
                        surface.release()
                        surfaceTexture.release()
                        result.success(mapOf("error" to "Failed to Create Video"))
                    } else {
                        resourceHolder[surfaceTexture.id()] = FlNativeVideo(surfaceTexture, surface, pointer)

                        result.success(
                            mapOf(
                                "textureId" to surfaceTexture.id(),
                            )
                        )
                    }

                }


                "flNativeVideoDelete" -> {
                    val textureId = call.arguments<Long>()!!
                    val resource = resourceHolder.remove(textureId)
                    if (resource != null) {
                        shutdown(resource.nativePointer)
                        resource.entry.release()
                        resource.surface.release()
                        result.success(0)
                    } else {
                        result.success(-1)
                    }
                }


                "flNativeVideoSetPlayMode" -> {
                    val args = call.arguments<Map<String, Any>>()!!
                    val isPlaying = args["isPlaying"] as Boolean
                    val textureId = args["textureId"] as Int
                    val resource = resourceHolder[textureId.toLong()]
                    if (resource != null) {
                       setPlayingStreamingMediaPlayer(isPlaying, resource.nativePointer)
                        result.success(0)
                    } else {
                        result.success(1)
                    }
                }

                else -> result.notImplemented()
            }

        }
    }
}
