# kg_LayoutGrid AEX

After Effects SDK native plug-in that creates a Figma-like stretch layout grid for the active composition.

The AEX contains two registrations:

- AEGP command: `Window > kg_LayoutGrid...`
- PF effect: `Effect > KG_plugins > kg_LayoutGrid`

The PF effect draws the layout grid directly on the layer it is applied to. The applied layer is also used as the controller by storing generated guide history in its layer comment.

## Build

- Target: Windows x64 `.aex`
- Toolchain: Visual Studio 2022, MSVC v143
- SDK layout: this repository's `Examples/Headers`, `Examples/Util`, and `Examples/Resources/PiPLTool`

Build:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" Examples\kg_plugins\kg_LayoutGrid\Win\kg_LayoutGrid.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

Output:

```text
Examples/kg_plugins/build/Release/kg_LayoutGrid.aex
```

Install by copying the `.aex` to an After Effects Plug-ins folder, for example:

```text
C:\Program Files\Adobe\Adobe After Effects 2025\Support Files\Plug-ins\
```

## Use

1. Apply `Effect > KG_plugins > kg_LayoutGrid` to a solid or layer.
2. Set Columns, Rows, Band Opacity, Column Color, Row Color, and Spiral Color in Effect Controls.
3. Use `Apply Guides` to create AE standard guides from the current settings.
4. Use `Clear Grid Guides` or `Clear All` to remove guides created by this effect.

The Window menu command remains available as a utility dialog, but the primary workflow is now the Effect Controls UI.

## Implementation Notes

The main tool is a native AEGP command plug-in with a small Win32 dialog. AE composition changes are executed through `AEGP_ExecuteScript`. This is intentional because this SDK does not expose a stable direct AEGP suite for AE standard guide creation/removal, while ExtendScript provides `comp.addGuide()` and `comp.removeGuide()`.

A PF effect entry point renders transparent column and row bands. Overlapping row/column areas become darker because the bands are alpha-composited. Guide operations still use `AEGP_ExecuteScript` because AE standard guides are not exposed through a stable direct AEGP suite.

Grid math and validation are plain C++ in `src/GridMath.*` and `src/GridValidation.*`.

## Known Limits

- This is a command utility under the Window menu, not a dockable AE panel.
- The applied effect draws transparent grid bands on the target layer; it does not create a separate preview layer.
- AE standard guides are not live Expression-controlled.
- Standard guides require `Apply Guides`.
- AE standard guides have no names, so cleanup matches `orientation + position`.
- If a manual guide exists at exactly the same orientation and position as a generated guide, AE cannot distinguish it.
- macOS is not implemented yet; the AEGP entry and grid logic are portable, but UI needs a macOS implementation.
