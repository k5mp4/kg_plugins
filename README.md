# kg_LayoutGrid

[日本語](#日本語) / [English](#english)

## 日本語

After Effects SDK ベースのレイアウトグリッドプラグインです。

## 構成

- `kg_plugins.sln` - `kg_LayoutGrid` 用の Visual Studio ソリューション
- `kg_LayoutGrid` - プラグインのソースコードと Visual Studio プロジェクトファイル

このリポジトリは、以下の場所に配置する想定です。

```text
AfterEffectsSDK/Examples/kg_plugins
```

プロジェクトファイルは、After Effects SDK の `Examples` 配下にあるヘッダーやユーティリティプロジェクトを相対パスで参照します。

## ビルド

Visual Studio 2022 で `kg_plugins.sln` を開くか、MSBuild でビルドします。

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" .\kg_plugins.sln /p:Configuration=Release /p:Platform=x64 /m
```

ビルド生成物は Git の管理対象外です。ソースコードは Git で管理し、ビルド済みの `.aex` は GitHub Releases で公開します。

## リリース

詳しい手順は [RELEASE.md](RELEASE.md) を参照してください。

基本の流れは以下です。

1. `Release|x64` でビルドします。
2. 出力先から `.aex` ファイルを集めます。
3. `v0.1.0` のようなバージョンタグを作成します。
4. GitHub Releases に `.aex` ファイルまたは zip アーカイブを添付します。

## English

An After Effects SDK based layout grid plugin.

## Layout

- `kg_plugins.sln` - Visual Studio solution for `kg_LayoutGrid`
- `kg_LayoutGrid` - plugin source code and Visual Studio project files

This repository is expected to be placed under:

```text
AfterEffectsSDK/Examples/kg_plugins
```

The project files use relative paths to headers and utility projects under the After Effects SDK `Examples` directory.

## Build

Open `kg_plugins.sln` with Visual Studio 2022, or build it with MSBuild:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" .\kg_plugins.sln /p:Configuration=Release /p:Platform=x64 /m
```

Build artifacts are ignored by Git. Manage source changes in Git, and publish compiled `.aex` files through GitHub Releases.

## Release

See [RELEASE.md](RELEASE.md) for the detailed workflow.

Basic flow:

1. Build `Release|x64`.
2. Collect the generated `.aex` file.
3. Create a version tag such as `v0.1.0`.
4. Attach the `.aex` file or a zip archive to the matching GitHub Release.
