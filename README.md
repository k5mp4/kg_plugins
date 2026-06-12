# kg_plugins

[日本語](#日本語) / [English](#english)

## 日本語

`kg_plugins` は、ke-goによるAdobe AfterEffectsのプラグイン集です。
現時点ではMac未対応です。

このリポジトリは After Effects SDK の `Examples` 配下に配置してビルドする前提です。各 Visual Studio プロジェクトは、SDK に含まれるヘッダー、ユーティリティ、PiPLTool を相対パスで参照します。

現在のビルド環境は After Effects SDK 25.6 に更新済みです。

### 収録プラグイン

| ディレクトリ | 概要 |
| --- | --- |
| `kg_LayoutGrid` | Figma ライクなストレッチレイアウトグリッドをコンポジション上に描画し、AE標準ガイドとして適用できるプラグインです。 |

各プラグインの詳細は、それぞれのディレクトリ内のREADMEを参照してください。

### 配置

```text
AfterEffectsSDK/Examples/kg_plugins
```

### ビルド

Visual Studio 2022 で `kg_plugins.sln` を開くか、MSBuild でビルドします。

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" .\kg_plugins.sln /p:Configuration=Release /p:Platform=x64 /m
```

生成された `.aex` は `build/Release` などのビルド出力ディレクトリに配置されます。ビルド生成物はGit管理対象外です。

### ライセンス

このリポジトリのソースコードは Apache License 2.0 の下で公開されています。詳細は [LICENSE](LICENSE) を参照してください。

Adobe After Effects SDK 自体はAdobeのライセンスに従います。このリポジトリはSDK本体を再配布しません。

## English

`kg_plugins` is a collection of native plug-ins built with the Adobe After Effects SDK.

This repository is intended to be placed under the After Effects SDK `Examples` directory. The Visual Studio projects reference SDK headers, utilities, and PiPLTool through relative paths.

The current build environment has been updated to After Effects SDK 25.6.

### Included Plug-ins

| Directory | Description |
| --- | --- |
| `kg_LayoutGrid` | Draws a Figma-like stretch layout grid in a composition and can apply it as standard After Effects guides. |
| `kg_DiffuseBlur` | After Effects SDK based effect plug-in. |
| `kg_Ghost` | After Effects SDK based effect plug-in. |
| `kg_Halo` | After Effects SDK based effect plug-in. |
| `kg_Halo2` | After Effects SDK based effect plug-in. |
| `kg_LineAnim` | After Effects SDK based effect plug-in. |
| `kg_Thread` | After Effects SDK based effect plug-in. |

See each plug-in directory for plug-in-specific documentation.

### Location

```text
AfterEffectsSDK/Examples/kg_plugins
```

### Build

Open `kg_plugins.sln` with Visual Studio 2022, or build it with MSBuild:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" .\kg_plugins.sln /p:Configuration=Release /p:Platform=x64 /m
```

Generated `.aex` files are written to build output directories such as `build/Release`. Build artifacts are ignored by Git.

### License

The source code in this repository is released under the Apache License 2.0. See [LICENSE](LICENSE) for details.

The Adobe After Effects SDK itself is governed by Adobe's license. This repository does not redistribute the SDK.
