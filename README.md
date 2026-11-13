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

The policy for this demo is to track the major.minor of haiven-sdk-unreal. A feature that shows part of the platform in Lyra ships as a demo patch, with no change to haiven-sdk-unreal. A breaking change that only affects this demo also ships as a demo patch, not as a minor or major release.

The release configuration does not enforce this policy yet. Today release-please versions this repository on its own from conventional commits ([release-please-config.json](https://github.com/haiven-dev/haiven-sdk-unreal-lyra/blob/main/release-please-config.json)), starting at 0.0.1, and nothing ties this version to haiven-sdk-unreal. Before 1.0, `bump-patch-for-minor-pre-major` turns a `feat:` commit into a patch bump, which matches the policy. But `bump-minor-pre-major` turns a breaking commit (`feat!:` or a `BREAKING CHANGE:` footer) into a minor bump, which does not. Until the configuration changes, add a `Release-As: x.y.z` footer that names the next patch version to the body of a demo-only breaking commit. release-please then uses that version instead of the minor bump. Pull requests into `sdk-dev` are squash-merged and release-please reads the squash commit, so the `Release-As:` line must be the last line of the squash commit message: make the breaking commit the pull request's last commit, or edit the message when you merge.

release-please writes this repository's version to `VersionName` in the HaivenLyraDemo and HaivenLyraInventory plugin descriptors, and `ci/release.py` checks a release tag against them. `Plugins/Haiven/Haiven.uplugin` is a copy of the haiven-sdk-unreal plugin, so its `VersionName` is the SDK version from the last plugin sync, and neither release-please nor `ci/release.py` changes or compares it.


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