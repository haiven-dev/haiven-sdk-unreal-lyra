Lyra Sample Game
===

Content Information
----
When downloading Lyra Source code from git, the content folders are not included.

To make use of the Lyra source code, you will need to download the content from the Unreal Marketplace through the Epic Games Launcher.

For more information please visit the [Unreal Engine Lyra documentation](https://docs.unrealengine.com/5.0/en-US/lyra-sample-game-in-unreal-engine/)

Once installed, copy the "Content" folder and any other folders named "Content" from the Marketplace project you downloaded to the project in your solution, and you should be good to go.


Additional Information
----
## Licensing and contributing

Lyra's Marketplace content is not in this git checkout, and there is no
`../../../README.md` here. The Epic sample is covered by the Unreal Engine
license that came with that Marketplace download. See the
[Lyra sample documentation](https://docs.unrealengine.com/5.0/en-US/lyra-sample-game-in-unreal-engine/).

Haiven changes in this repository are the plugins
[Haiven](Plugins/Haiven/Haiven.uplugin),
[HaivenLyraDemo](Plugins/HaivenLyraDemo/HaivenLyraDemo.uplugin), and
[HaivenLyraInventory](Plugins/HaivenLyraInventory/HaivenLyraInventory.uplugin).
Review those as GitHub pull requests. `.github/workflows/build.yml` checks the
three descriptors on pull requests and on a `v*` tag. It does not cook Lyra;
that still needs the Marketplace content and an Unreal install.