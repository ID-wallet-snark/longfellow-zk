# Longfellow ZK - Graphical User Interface

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](../LICENSE)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![ImGui](https://img.shields.io/badge/ImGui-docking-green.svg)](https://github.com/ocornut/imgui)
[![OpenGL](https://img.shields.io/badge/OpenGL-3.3+-orange.svg)](https://www.opengl.org/)

A modern, intuitive graphical interface for **zero-knowledge identity verification** using the Longfellow ZK proof system.

> **🚀 New here?** Check out the [**Quick Start Guide**](QUICKSTART.md) to get up and running in 5 minutes!

> **⚠️ Important:** Circuit generation takes 30-60 seconds and will freeze the UI. This is expected behavior for cryptographic compilation.

## Overview

This GUI application demonstrates privacy-preserving identity verification using zero-knowledge proofs. Users can prove attributes about their identity (such as age or nationality) without revealing the underlying personal data.

**Key Features:**
- 🔐 Zero-Knowledge proof generation for identity attributes
- ✓ Proof verification without data disclosure
- 🎯 Selective attribute disclosure (age, nationality, etc.)
- 🖥️ Clean, modern interface built with ImGui
- 🔒 Privacy-preserving by design

## Technology Stack

- **GUI Framework**: [Dear ImGui](https://github.com/ocornut/imgui) - Lightweight immediate mode GUI
- **Graphics**: OpenGL 3.3+ with GLFW and GLEW
- **ZK Backend**: Longfellow ZK proof system (mdoc circuits)
- **Build System**: CMake 3.13+
- **Language**: C++17

## Table of Contents

- [Quick Start](QUICKSTART.md) ⚡
- [Prerequisites](#prerequisites)
- [Building the Application](#building-the-application)
- [Using the Application](#using-the-application)
- [Architecture](#architecture)
- [Troubleshooting](#troubleshooting)
- [Known Issues](#known-issues)
- [FAQ](#frequently-asked-questions-faq)
- [Performance](#performance)
- [Contributing](#contributing)

## Prerequisites

Before building the GUI, ensure you have the following dependencies installed:

### System Dependencies

```bash
# Fedora/RHEL/CentOS
sudo dnf install cmake gcc-c++ openssl-devel zlib zstd-devel glfw-devel glew-devel mesa-libGL-devel

# Ubuntu/Debian
sudo apt install cmake g++ libssl-dev zlib1g-dev libzstd-dev libglfw3-dev libglew-dev libgl1-mesa-dev

# macOS (with Homebrew)
brew install cmake openssl zstd glfw glew
```

### ImGui Library

The project uses ImGui as a git submodule. It will be cloned automatically during the build process, or you can clone it manually:

```bash
cd gui
git clone https://github.com/ocornut/imgui.git
```

## Building the Application

### Quick Start

From the project root directory:

```bash
# Configure the build system
cmake .

# Build the GUI application
make longfellow_gui

# Run the application
./gui/longfellow_gui
```

### Detailed Build Steps

1. **Clone ImGui** (if not already present):
   ```bash
   cd gui
   git clone https://github.com/ocornut/imgui.git
   cd ..
   ```

2. **Build the core libraries**:
   ```bash
   cd lib
   cmake .
   make mdoc
   cd ..
   ```

3. **Build the GUI application**:
   ```bash
   cmake .
   make longfellow_gui
   ```

4. **Run the application**:
   ```bash
   ./gui/longfellow_gui
   ```

### Testing the ZK Backend

Before using the full GUI, you can test that the ZK circuit generation works:

```bash
# Build and run the test utility
make test_circuit
./gui/test_circuit
```

Expected output:
```
=== Longfellow ZK Circuit Generation Test ===

[Test 1] Available ZkSpecs:
  Spec #0: 1 attributes
  ...

[Test 2] Generate circuit for 1 attribute...
  Found ZkSpec #0
  Calling generate_circuit()...
  [Takes ~40 seconds - this is normal]
  SUCCESS: Circuit generated (278354 bytes)

=== All tests passed! ===
```

### Build Artifacts

After a successful build, you will find:
- **Executable**: `gui/longfellow_gui` (~2.7 MB)
- **Libraries**: `lib/circuits/mdoc/libmdoc.a` and related libraries

## Project Structure

```
gui/
├── CMakeLists.txt          # Build configuration
├── main.cpp                # Application entry point and UI logic
├── README.md               # This file
└── imgui/                  # ImGui library (git submodule)
    ├── imgui.cpp
    ├── imgui.h
    ├── backends/
    │   ├── imgui_impl_glfw.cpp
    │   └── imgui_impl_opengl3.cpp
    └── ...
```

## Using the Application

### Interface Overview

The GUI provides an intuitive interface with the following sections:

1. **Attributes to Prove**: Select which identity attributes you want to prove
   - Age verification (with configurable threshold)
   - Nationality verification

2. **Document Data**: For testing purposes, you can configure test document data
   - Birth date (year, month, day)

3. **Actions**:
   - **Generate ZK Proof**: Creates a zero-knowledge proof for selected attributes
   - **Verify Proof**: Validates the generated proof
   - **Clear**: Resets the interface and logs

4. **Activity Log**: Real-time feedback on operations and proof status

### Workflow Example

1. Launch the application: `./gui/longfellow_gui`
2. Select "Age Verification" and set the threshold (e.g., 18 years)
3. Optionally configure test birth date data
4. Click "🔐 Generate ZK Proof"
   - ⚠️ **Wait 30-60 seconds** - the UI will freeze during circuit generation
   - Watch the Activity Log for progress
5. Click "✓ Verify Proof" to validate the proof
6. Observe that personal data remains private!

### Testing Without GUI

If you want to test the ZK circuit generation without launching the full GUI:

```bash
# Build the test utility
make test_circuit

# Run the test (takes ~40 seconds)
./gui/test_circuit
```

This test utility:
- Lists available ZK specifications
- Generates a circuit for 1 attribute
- Displays timing information
- Tests attribute creation
- Exits after completion

## Architecture

### Component Integration

```
┌─────────────────────────────────────────┐
│          GUI Application                │
│         (main.cpp + ImGui)              │
├─────────────────────────────────────────┤
│      Longfellow ZK Libraries            │
│  - mdoc: Document proofs                │
│  - algebra: Finite field arithmetic     │
│  - ec: Elliptic curve operations        │
│  - util: Cryptographic utilities        │
└─────────────────────────────────────────┘
```

### Key Functions

- `CreateAgeAttribute()`: Creates a requested attribute for age verification
- `CreateNationalityAttribute()`: Creates a requested attribute for nationality
- `GenerateZKProof()`: Orchestrates ZK proof generation
- `VerifyZKProof()`: Validates a generated proof
- `RenderMainWindow()`: Main UI rendering loop

## Troubleshooting

### Build Issues

**Problem**: CMake can't find ImGui files
```
CMake Error: Cannot find source file: imgui/imgui.cpp
```
**Solution**: Clone ImGui into the `gui/` directory:
```bash
cd gui && git clone https://github.com/ocornut/imgui.git
```

---

**Problem**: Linker errors about missing symbols
```
undefined reference to `generate_circuit'
```
**Solution**: Build from the project root, not from `gui/`:
```bash
cd /path/to/longfellow-zk
cmake .
make longfellow_gui
```

---

**Problem**: OpenSSL deprecation warnings
```
warning: 'SHA256_Init' is deprecated
```
**Solution**: These are warnings, not errors. The build will succeed. To suppress them, add `-Wno-deprecated-declarations` to compiler flags.

### Runtime Issues

**Problem**: Application fails to start with OpenGL errors
**Solution**: Ensure you have OpenGL 3.3+ support and proper graphics drivers installed.

**Problem**: Window doesn't appear
**Solution**: Check that GLFW and display server (X11/Wayland) are properly configured.

---

**Problem**: GUI freezes for 30-60 seconds when clicking "Generate ZK Proof"
**Solution**: This is **normal behavior**. Circuit generation is a CPU-intensive cryptographic operation. The UI will become responsive again once complete. Watch the Activity Log for progress.

---

**Problem**: Application crashes during circuit generation
**Solution**: 
1. First, test with the CLI utility: `./gui/test_circuit`
2. Check available RAM (needs ~500 MB during generation)
3. Look for error messages in the Activity Log
4. Run from terminal to see console output

## Known Issues

### UI Freeze During Circuit Generation

**Issue**: The graphical interface becomes unresponsive for 30-60 seconds when generating a ZK proof.

**Why**: Circuit generation is a synchronous, CPU-intensive operation that runs on the main thread.

**Workaround**: 
- Be patient and wait for completion
- Watch the Activity Log for progress messages
- The interface will automatically become responsive once generation completes

**Future Fix**: Move circuit generation to a background thread with a progress indicator.

### Circuit Generation Cannot Be Cancelled

**Issue**: Once you click "Generate ZK Proof", you must wait for completion. There is no cancel button.

**Workaround**: Close the application window if needed (safe to do).

**Future Fix**: Add cancellation support and background threading.

### No Circuit Caching

**Issue**: Every proof generation recompiles the circuit, even for the same attributes.

**Impact**: Each proof generation takes 30-60 seconds, even if you've generated the same proof before.

**Future Fix**: Cache compiled circuits on disk for reuse.

### OpenSSL Deprecation Warnings

**Issue**: Compilation shows warnings about deprecated OpenSSL functions (SHA256_Init, etc.).

**Impact**: None - these are warnings, not errors. The code compiles and runs correctly.

**Status**: This is an upstream issue in the Longfellow library.

## Frequently Asked Questions (FAQ)

### Q: Why does the GUI freeze when I click "Generate ZK Proof"?

**A**: Circuit generation is a CPU-intensive cryptographic operation that takes 30-60 seconds. The current implementation runs on the main thread, causing the UI to freeze. This is expected behavior. The UI will become responsive once generation completes.

### Q: How long should I expect to wait?

**A**: 
- **First-time circuit generation**: 30-60 seconds (compiles cryptographic circuits)
- **Proof creation**: Instant (after circuit is generated)
- **Proof verification**: <1 second

### Q: Can I speed up circuit generation?

**A**: Currently, no. Circuit generation involves:
- Compiling signature verification circuits (~3 seconds)
- Compiling hash computation circuits (~30 seconds)
- Circuit compression (~6 seconds)

Future optimizations could include circuit caching and pre-generation.

### Q: Is my data secure?

**A**: Yes! This application demonstrates **zero-knowledge proofs**:
- Your personal data never leaves your device
- Only cryptographic proofs are generated
- Verifiers learn only what you explicitly prove (e.g., "age > 18")
- No personal information is revealed in the proof

### Q: Can I use this in production?

**A**: This is a **demonstration application**. For production use, you should:
- Implement proper key management
- Pre-generate and cache circuits
- Move proof generation to a background service
- Add proper error handling and logging
- Use secure channels for proof transmission
- Follow security best practices

### Q: Why do I see OpenSSL warnings during compilation?

**A**: These are deprecation warnings from OpenSSL 3.0. They don't affect functionality - the code compiles and runs correctly. This is an upstream issue in the Longfellow library.

### Q: What attributes can I prove?

**A**: Currently supported:
- **Age verification**: Prove you're above a certain age threshold (13-25 years)
- **Nationality**: Prove your nationality matches a specific country code

Additional attributes can be added by extending the code.

### Q: How do I test without the GUI?

**A**: Use the command-line test utility:
```bash
make test_circuit
./gui/test_circuit
```

This tests the ZK backend without requiring a graphical display.

### Q: Why does it take so much memory?

**A**: Circuit generation creates large cryptographic circuits:
- Base memory: ~50-100 MB
- During generation: up to 500 MB
- Uncompressed circuits: ~88 MB (compressed to ~278 KB)

This is normal for zero-knowledge proof systems.

### Q: Can I cancel circuit generation?

**A**: Currently, no. Once started, you must wait for completion or close the application. Future versions could add cancellation support.

### Q: Where are the circuits stored?

**A**: Currently, circuits are generated in memory and discarded after use. They are not cached. Future versions could implement disk caching for better performance.

## Development

### Code Style

- Follow C++17 standards
- Use descriptive variable names
- Comment complex ZK operations
- Keep UI logic separate from proof logic

### Adding New Attributes

To add a new attribute for verification:

1. Create a new `CreateXAttribute()` function
2. Add UI controls in `RenderMainWindow()`
3. Update `GenerateZKProof()` to handle the new attribute
4. Ensure proper ZkSpec exists for the attribute combination

### Debugging

Enable verbose logging:
```cpp
proofs::set_log_level(proofs::DEBUG);
```

## Security Considerations

⚠️ **Important**: This is a demonstration application for educational purposes.

- The example uses test mdoc documents
- In production, implement proper key management
- Validate all user inputs
- Use secure channels for proof transmission
- Follow best practices for cryptographic operations

## Performance

⚠️ **Important Performance Notes:**

- **Circuit generation**: ~30-60 seconds (first-time compilation of ZK circuits)
  - This is a one-time operation per attribute configuration
  - The UI will freeze during this time (synchronous operation)
  - Produces ~278 KB compressed circuit data (~88 MB uncompressed)
- **Proof generation**: After circuit is generated, proof creation is fast
- **Proof verification**: <1 second
- **Memory usage**: ~50-100 MB base, up to 500 MB during circuit generation
- **CPU**: Single-threaded (optimization opportunity)

### Why is circuit generation slow?

The circuit generation compiles cryptographic circuits for:
1. **Signature verification circuit** (~6 MB, ~3 seconds)
2. **Hash computation circuit** (~88 MB, ~30 seconds)
3. **Circuit compression** (~6 seconds)

This is a necessary cryptographic operation and is normal for ZK proof systems.

### Recommendations for Production Use

For production deployments, consider:
1. **Pre-generate circuits** for common attribute combinations
2. **Cache circuits** on disk to avoid regeneration
3. **Use a background service** for proof generation
4. **Implement circuit serving** over a network API
5. **Add progress indicators** for better UX

## Contributing

Contributions are welcome! Areas for improvement:

- [ ] Additional attribute types
- [ ] Real mdoc document integration
- [ ] **Async circuit generation** (move to background thread to prevent UI freeze)
- [ ] Circuit caching (avoid regenerating for same attributes)
- [ ] Progress bar during circuit generation
- [ ] Export/import proof functionality
- [ ] Internationalization (i18n)
- [ ] Dark/light theme toggle
- [ ] Headless mode for server deployment

## License

Copyright 2025 Google LLC

Licensed under the Apache License, Version 2.0. See the LICENSE file in the project root for details.

## References

- [Longfellow ZK Documentation](../docs/)
- [Dear ImGui Documentation](https://github.com/ocornut/imgui)
- [ISO 18013-5 mDL Standard](https://www.iso.org/standard/69084.html)
- [Zero-Knowledge Proofs](https://en.wikipedia.org/wiki/Zero-knowledge_proof)

## Support

For issues, questions, or contributions:
- Open an issue on the project repository
- Consult the main project documentation in `docs/`
- Review the reference implementations in `reference/`

---

**Built with ❤️ for privacy-preserving identity verification**