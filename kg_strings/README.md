# kg_strings AEX

[日本語](#日本語) / [English](#english)

## 日本語

`kg_strings` は、After Effects の選択レイヤーパスに沿って複数のストリング状ラインを生成・アニメーションする PF エフェクトプラグインです。

この `.aex` は次のエフェクトとして登録されます。

- PFエフェクト: `Effect > kg_plugins > kg_strings`

パスパラメーターを最大8本まで参照し、ライン数、間隔、幅、トリム、ダッシュ、テーパー、カラー、Web接続、ノイズ、オーディオリアクティブ表現をEffect Controlsから調整できます。

### ビルド

- ターゲット: Windows x64 `.aex`
- ツールチェーン: Visual Studio 2022、MSVC v143
- SDKバージョン: After Effects SDK 25.6
- SDK配置: このリポジトリから見た `Examples/Headers`、`Examples/Util`、`Examples/Resources/PiPLTool`

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" Examples\kg_plugins\kg_strings\Win\kg_strings.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

出力先:

```text
Examples/kg_plugins/build/Release/kg_strings.aex
```

インストールするには、生成された `.aex` をAfter EffectsのPlug-insフォルダーにコピーします。

```text
C:\Program Files\Adobe\Adobe After Effects 2025\Support Files\Plug-ins\
```

### 使い方

1. マスクパスまたはシェイプパスを持つレイヤーを用意します。
2. `Effect > kg_plugins > kg_strings` をレイヤーに適用します。
3. `Path` グループで参照するパスを指定します。
4. `Line Setup` で Line Count、Spacing、Width、Path Offset などを調整します。
5. 必要に応じて `Trim`、`Dash`、`Taper`、`Style`、`Web`、`Noise`、`Audio Reactive`、`Grunge` を調整します。

### 実装メモ

エントリーポイントは `kg_strings.cpp` で、実装は `src/` 配下の複数ファイルを include する構成です。

- `kg_strings_core.cpp`: 共通ユーティリティ、描画用データの収集
- `kg_strings_params.cpp`: エフェクトパラメーター定義
- `kg_strings_paths.cpp`: パスサンプリングとライン配置
- `kg_strings_web.cpp`: Web接続パターン
- `kg_strings_ui.cpp`: カスタムUIイベント
- `kg_strings_render.cpp`: レンダリング処理

`PF_OutFlag_CUSTOM_UI`、`PF_OutFlag_DEEP_COLOR_AWARE`、`PF_OutFlag_I_USE_AUDIO` を使い、カスタムUI、Deep Color、オーディオ入力に対応します。3Dカメラ情報も参照できるように `PF_OutFlag2_I_USE_3D_CAMERA` を設定しています。

### 既知の制限

- Windows x64ビルドのみをプロジェクトに含めています。
- macOSは未実装です。PiPL側のエントリーは残っていますが、ビルドプロジェクトはありません。
- パスの品質と負荷は `Curve Segments`、Line Count、Web/Noise/Audio設定に依存します。
- オーディオリアクティブ表現には参照するオーディオレイヤーの指定が必要です。

### ライセンス

このプラグインはリポジトリルートの [LICENSE](../LICENSE) に記載された Apache License 2.0 の下で公開されています。

## English

`kg_strings` is an After Effects PF effect plug-in that generates and animates multiple string-like lines along selected layer paths.

The `.aex` registers the following effect:

- PF effect: `Effect > kg_plugins > kg_strings`

It can reference up to eight path parameters and exposes controls for line count, spacing, width, trim, dashes, taper, color, web connections, noise, and audio-reactive motion in Effect Controls.

### Build

- Target: Windows x64 `.aex`
- Toolchain: Visual Studio 2022, MSVC v143
- SDK version: After Effects SDK 25.6
- SDK layout: this repository's `Examples/Headers`, `Examples/Util`, and `Examples/Resources/PiPLTool`

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" Examples\kg_plugins\kg_strings\Win\kg_strings.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

Output:

```text
Examples/kg_plugins/build/Release/kg_strings.aex
```

Install by copying the generated `.aex` to an After Effects Plug-ins folder.

```text
C:\Program Files\Adobe\Adobe After Effects 2025\Support Files\Plug-ins\
```

### Use

1. Prepare a layer with mask paths or shape paths.
2. Apply `Effect > kg_plugins > kg_strings` to the layer.
3. Select path references in the `Path` group.
4. Adjust Line Count, Spacing, Width, Path Offset, and related controls in `Line Setup`.
5. Use `Trim`, `Dash`, `Taper`, `Style`, `Web`, `Noise`, `Audio Reactive`, and `Grunge` as needed.

### Similar Plug-ins

`kg_strings` is conceptually close to line, path, particle, and network-style effects such as Rowbyte Plexus, Maxon Trapcode 3D Stroke, Maxon Trapcode Form/Particular, and Stardust. Its focus is narrower: it generates multiple animated string-like lines from After Effects mask or shape paths, with direct controls for spacing, trim, taper, web connections, noise, and audio-reactive motion.

### Implementation Notes

The entry point is `kg_strings.cpp`, which includes the implementation files under `src/`.

- `kg_strings_core.cpp`: shared utilities and render-info collection
- `kg_strings_params.cpp`: effect parameter setup
- `kg_strings_paths.cpp`: path sampling and line placement
- `kg_strings_web.cpp`: web connection patterns
- `kg_strings_ui.cpp`: custom UI events
- `kg_strings_render.cpp`: rendering

The effect uses `PF_OutFlag_CUSTOM_UI`, `PF_OutFlag_DEEP_COLOR_AWARE`, and `PF_OutFlag_I_USE_AUDIO` for custom controls, Deep Color, and audio input. It also sets `PF_OutFlag2_I_USE_3D_CAMERA` so render code can reference 3D camera data.

### Known Limits

- Only the Windows x64 build project is included.
- macOS is not implemented. PiPL entries remain present, but no macOS project is provided.
- Path quality and render cost depend on `Curve Segments`, Line Count, and Web/Noise/Audio settings.
- Audio-reactive rendering requires an audio layer reference.

### License

This plug-in is released under the Apache License 2.0 described in the repository root [LICENSE](../LICENSE).
