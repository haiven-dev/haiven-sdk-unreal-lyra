Lyra Sample Game
===

Content Information
----
When downloading Lyra Source code from git, the content folders are not included.

To make use of the Lyra source code, you will need to download the content from the Unreal Marketplace through the Epic Games Launcher.

For more information please visit the [Unreal Engine Lyra documentation](https://docs.unrealengine.com/5.0/en-US/lyra-sample-game-in-unreal-engine/)

Once installed, copy the "Content" folder and any other folders named "Content" from the Marketplace project you downloaded to the project in your solution, and you should be good to go.


## Haiven versioning

[haiven-sdk-unreal](https://github.com/haiven-dev/haiven-sdk-unreal) follows semver: a feature ships in a minor release and a fix ships in a patch.

The intended policy for this demo is to track the major.minor of haiven-sdk-unreal and to let a feature that shows part of the platform in Lyra ship as a demo patch, with no change to haiven-sdk-unreal. The release configuration does not enforce that yet. Today release-please versions this repository on its own from conventional commits ([release-please-config.json](https://github.com/haiven-dev/haiven-sdk-unreal-lyra/blob/main/release-please-config.json)), starting at 0.0.1. Before 1.0, `bump-patch-for-minor-pre-major` turns a `feat:` commit into a patch bump. Nothing ties this version to haiven-sdk-unreal.


Additional Information
----
See the [Unreal Engine README](../../../README.md) at the root of the repository for [Licensing](../../../README.md#licensing) and [Contributing](../../../README.md#contributions) information.