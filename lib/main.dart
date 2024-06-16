import 'dart:io';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter_android_video_test/native_codec.dart';
import 'package:path/path.dart' as p;
import 'package:path_provider/path_provider.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      showPerformanceOverlay: true,
      title: 'Flutter Demo',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.deepPurple),
        useMaterial3: true,
      ),
      home: const MyHomePage(title: 'Flutter Demo Home Page'),
    );
  }
}

class MyHomePage extends StatefulWidget {
  const MyHomePage({super.key, required this.title});

  final String title;

  @override
  State<MyHomePage> createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {
  final filePaths = <String>[];

  int _counter = 1;

  void _incrementCounter() {
    setState(() {
      _counter++;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        backgroundColor: Theme.of(context).colorScheme.inversePrimary,
        title: Text(widget.title),
      ),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: <Widget>[
            Text(
              'Next Page will have ListView.builder itemCount: $_counter times',
            ),
            TextButton(
              onPressed: () async {
                final result = await FilePicker.platform
                    .pickFiles(allowMultiple: true, type: FileType.video);

                final cacheDir = await getApplicationDocumentsDirectory();
                final files = result?.files ?? [];
                final futures = <Future>[];
                for (var file in files) {
                  if (file.path != null) {
                    final newPath = p.join(cacheDir.path, file.name);
                    // The file picker picked path gets cleaned up after some time, so making a copy for use
                    futures.add(File(file.path!).copy(newPath));
                    filePaths.add(newPath);
                  }
                }
                await Future.wait(futures);
                setState(() {});
              },
              child: const Text("Pick Videos "),
            ),
            TextButton(
                onPressed: () => launchVideosPage(VideoImpl.exoPlayer),
                child: const Text("Default Exo Player")),
            TextButton(
                onPressed: () => launchVideosPage(VideoImpl.nkdMediaCodec),
                child: const Text("Android NDK Media Codec Video Player")),
            TextButton(
                onPressed: () => launchVideosPage(VideoImpl.vlc),
                child: const Text("VLC Video Player")),
            Padding(
              padding: const EdgeInsets.all(8.0),
              child: Column(
                children: [for (var filePath in filePaths) Text(filePath)],
              ),
            ),
          ],
        ),
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: _incrementCounter,
        tooltip: 'Increment',
        child: const Icon(Icons.add),
      ),
    );
  }

  void launchVideosPage(VideoImpl implementation) {
    if (filePaths.isNotEmpty) {
      Navigator.of(context).push(
        MaterialPageRoute(
          builder: (_) => VideoPage(
            filePaths,
            itemCount: _counter,
            backend: implementation,
          ),
        ),
      );
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text("Pick Files for testing")));
    }
  }
}

enum VideoImpl { exoPlayer, nkdMediaCodec, vlc }
