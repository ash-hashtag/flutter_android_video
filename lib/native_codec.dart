import 'dart:io';
import 'dart:math';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_android_video_test/main.dart';
import 'package:flutter_vlc_player/flutter_vlc_player.dart';
import 'package:video_player/video_player.dart';

class NativeCodecMethodChannel {
  static const channel = MethodChannel("fl_video");

  static Future<int?> flNativeVideoFromfile(String filePath) async {
    final Map result = await channel
        .invokeMethod("flNativeVideoFromfile", {"filePath": filePath});

    if (result["error"] == null) {
      return result["textureId"] as int;
    } else {
      return null;
    }
  }

  static Future<void> flNativeVideoDelete(int textureId) {
    return channel.invokeMethod("flNativeVideoDelete", textureId);
  }

  static Future<void> flNativeVideosetPlaying(int textureId, bool isPlaying) {
    return channel.invokeMethod("flNativeVideoSetPlayMode",
        {"textureId": textureId, "isPlaying": isPlaying});
  }
}

class NativeVideoPlayer extends StatefulWidget {
  final String filePath;
  const NativeVideoPlayer(this.filePath, {super.key});

  @override
  State<NativeVideoPlayer> createState() => _NativeVideoPlayerState();
}

class _NativeVideoPlayerState extends State<NativeVideoPlayer> {
  int? _textureId;
  bool isPlaying = false;

  Future<void> setupCodecs() async {
    final tId =
        await NativeCodecMethodChannel.flNativeVideoFromfile(widget.filePath);

    if (tId != null) {
      setState(() {
        _textureId = tId;
      });
      // NativeCodecMethodChannel.flNativeVideosetPlaying(tId, isPlaying);
    } else {
      debugPrint("======================");
      debugPrint("Failed creating video player view");
      debugPrint("======================");
    }
  }

  @override
  void initState() {
    super.initState();
    setupCodecs();
  }

  @override
  void dispose() {
    super.dispose();
    if (_textureId != null) {
      NativeCodecMethodChannel.flNativeVideoDelete(_textureId!);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      height: 200,
      width: double.infinity,
      color: Colors.black,
      child: _textureId != null
          ? _buildView()
          : const Center(child: Text("Creating Texture View")),
    );
  }

  Widget _buildView() {
    return Stack(
      children: [
        AspectRatio(
          aspectRatio: 16 / 9,
          child: Texture(
            textureId: _textureId!,
          ),
        ),
        Positioned(
          bottom: 10,
          left: 10,
          child: TextButton(
              onPressed: () => setState(() {
                    isPlaying = !isPlaying;
                    NativeCodecMethodChannel.flNativeVideosetPlaying(
                        _textureId!, isPlaying);
                  }),
              child: Text(isPlaying ? "Pause" : "Play")),
        ),
      ],
    );
  }
}

class VideoPage extends StatefulWidget {
  final List<String> filePaths;
  final VideoImpl backend;
  final int itemCount;

  const VideoPage(this.filePaths,
      {this.itemCount = 5, required this.backend, super.key});

  @override
  State<VideoPage> createState() => _VideoPageState();
}

class _VideoPageState extends State<VideoPage> {
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text("Video Page"),
      ),
      body: ListView.builder(
        itemCount: widget.itemCount,
        cacheExtent: 1000,
        itemBuilder: (context, _) => Padding(
          padding: const EdgeInsets.all(5.0),
          child: getVideoPlayer(_),
        ),
      ),
    );
  }

  Widget getVideoPlayer(int index) {
    final filePath =
        widget.filePaths[Random().nextInt(widget.filePaths.length)];
    switch (widget.backend) {
      case VideoImpl.exoPlayer:
        return DefaultVideoPlayer(filePath);
      case VideoImpl.nkdMediaCodec:
        return NativeVideoPlayer(filePath);
      case VideoImpl.vlc:
        return VlcVideoPlayer(filePath);
      default:
        throw UnimplementedError();
    }
  }
}

class DefaultVideoPlayer extends StatefulWidget {
  final String filePath;
  const DefaultVideoPlayer(this.filePath, {super.key});

  @override
  State<DefaultVideoPlayer> createState() => _DefaultVideoPlayerState();
}

class _DefaultVideoPlayerState extends State<DefaultVideoPlayer> {
  late final _controller = VideoPlayerController.file(File(widget.filePath),
      videoPlayerOptions: VideoPlayerOptions(mixWithOthers: true));

  @override
  void initState() {
    super.initState();
    _controller.initialize().then((_) {
      setState(() {});
    });
  }

  @override
  void dispose() {
    super.dispose();
    _controller.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      height: 200,
      width: double.infinity,
      color: Colors.black,
      child: _controller.value.isInitialized
          ? _buildView()
          : const Center(child: Text("Initializing Video Player")),
    );
  }

  Widget _buildView() {
    return Stack(
      children: [
        AspectRatio(
          aspectRatio: 16 / 9,
          child: VideoPlayer(_controller),
        ),
        Positioned(
          bottom: 10,
          left: 10,
          child: TextButton(
              onPressed: () => setState(() {
                    _controller.value.isPlaying
                        ? _controller.pause()
                        : _controller.play();
                  }),
              child: Text(_controller.value.isPlaying ? "Pause" : "Play")),
        ),
      ],
    );
  }
}

class VlcVideoPlayer extends StatefulWidget {
  final String filePath;
  const VlcVideoPlayer(this.filePath, {super.key});

  @override
  State<VlcVideoPlayer> createState() => _VlcVideoPlayerState();
}

class _VlcVideoPlayerState extends State<VlcVideoPlayer> {
  late final _controller = VlcPlayerController.file(File(widget.filePath),
      autoPlay: false, autoInitialize: true);

  @override
  void initState() {
    super.initState();

    _controller.addOnInitListener(() => setState(() {
          debugPrint("VLC Player initialized");
        }));
  }

  @override
  void dispose() {
    super.dispose();
    _controller.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Container(
        height: 200,
        width: double.infinity,
        color: Colors.black,
        child: _buildView());
  }

  Widget _buildView() {
    return Stack(
      children: [
        VlcPlayer(
            aspectRatio: 16 / 9,
            controller: _controller,
            placeholder:
                const Center(child: Text("Initializing VLC Video Player"))),
        Positioned(
          bottom: 10,
          left: 10,
          child: TextButton(
              onPressed: () async {
                final f = _controller.value.isPlaying
                    ? _controller.pause()
                    : _controller.play();
                await f;
                setState(() {});
              },
              child: Text(_controller.value.isPlaying ? "Pause" : "Play")),
        ),
      ],
    );
  }
}
