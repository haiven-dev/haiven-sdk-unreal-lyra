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
See the [Unreal Engine README](../../../README.md) at the root of the repository for [Licensing](../../../README.md#licensing) and [Contributing](../../../README.md#contributions) information.

## Haiven versioning

haiven-sdk and haiven-sdk-unreal follow semver. A feature of the Unreal target ships in the shared major.minor release, in parity with the other target SDKs. A patch of haiven-sdk-unreal is a fix and can ship on its own.

This demo, haiven-sdk-unreal-lyra, publishes that same major.minor and does not follow semver. A feature that shows part of the platform in Lyra can be introduced in a patch. That patch leaves haiven-sdk and haiven-sdk-unreal unchanged.

The proposal is [haiven-docs ADR 0051](https://github.com/haiven-dev/haiven-docs/blob/docs/adr-0051-major-minor-lockstep/decisions/0051-major-minor-lockstep.md).
