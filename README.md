# Dear ImGui Integration Sample

[**Dear ImGui**](https://github.com/ocornut/imgui) is a fast, minimalistic, and highly portable immediate-mode GUI library primarily used for creating in-game and real-time development tools. It's designed to be simple to integrate into existing applications and is especially popular in the game development, graphics, and visualization communities.

Rather than creating traditional GUI layouts, **Dear ImGui** allows developers to quickly build dynamic interfaces - perfect for debugging tools, editors, data visualizations, and real-time control panels. It focuses on responsiveness and ease of use, making it ideal for prototyping and tools where performance and simplicity matter.

## How to Run the Sample

### Prerequisites

- [**UNIGINE SDK Browser**](https://developer.unigine.com/en/docs/latest/start/installing_sdk?rlang=cpp) (latest version)
- **UNIGINE SDK Community** or **Engineering** edition (**Sim** upgrade supported)
- **Visual Studio 2022** (recommended)

### Step-by-Step Guide
The C++ sample uses **Dear ImGui** library version **v1.81** (https://github.com/ocornut/imgui/releases/tag/v1.81).

To get started with the **Dear Imgui C++ Sample**:

1. **Clone or download** the sample.

2. **Open SDK Browser** and make sure you have the latest version.

3. **Add the sample project to SDK Browser**:
   - Go to the *My Projects* tab.
   - Click *Add Existing*, select the `.project` file from the cloned folder (matching your OS - `*-win-*`/`*-lin-*`, edition, precision), and click *Import Project*.

     ![Add Project](https://developer.unigine.com/en/docs/latest/sdk/api_samples/third_party/photon/add_project.png)

> [!NOTE]
> If you're using **UNIGINE SDK *Sim***, select the ***Engineering*** `*-eng-sim-*.project` file when importing the sample. After import, you can upgrade the project to the **Sim** version directly in SDK Browser - just click *Upgrade*, choose the SDK **Sim** version, and adjust any additional settings you want to use in the configuration window that opens.

4. **Repair the project**:
   - After importing, you'll see a **Repair** warning - this is expected, as only essential files are stored in the Git repository. SDK Browser will restore the rest.

   ![Repair Project](https://developer.unigine.com/en/docs/latest/sdk/api_samples/third_party/repair_project.png)
   - Click *Repair* and then *Configure Project*.

6. **Open** the project in your IDE:
    - Launch the recommended Visual Studio 2022 (other C++ IDE with CMake support can be used as well).
    - Load the folder containing `CMakeLists.txt`. If everything is set up correctly, the `CMakeLists.txt` file will be highlighted in **bold** in the *Solution Explorer* window, indicating that the project is ready to build.

> [!WARNING]
   > By default, the project uses **single precision (float)**. To enable **double precision**, modify `CMakeLists.txt`:
   >```diff
   > - set(UNIGINE_DOUBLE False CACHE BOOL "Double coords")
   > + set(UNIGINE_DOUBLE True CACHE BOOL "Double coords")
   >```
   > Make sure this setting matches the `.project` file you selected (e.g., `*-double.project` for double precision builds).

6. **Build** and **Run** the project.

### If the sample fails to run:
- Double-check all setup steps above to ensure nothing was skipped.
- Check that `UNIGINE_DOUBLE` in `CMakeLists.txt` matches the current build type (double/float).
- Ensure the correct `.project` file is used for your platform and SDK edition.
- Verify your SDK version is not older than the project's specified version.
- If CMake issues occur in Visual Studio, right-click the project and select: **Delete Cache and Reconfigure** and then **Build** again.