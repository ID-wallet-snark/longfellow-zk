# Changelog - Longfellow ZK GUI

All notable changes to the GUI application will be documented in this file.

## [1.0.0] - 2024-11-18

### Added

#### Core Features
- **Zero-Knowledge Proof GUI Application**: Complete graphical interface for privacy-preserving identity verification
- **Age Verification**: Prove age threshold (13-25 years) without revealing exact birth date
- **Nationality Verification**: Prove nationality without revealing additional personal information
- **Real-time Activity Log**: Monitor proof generation and verification progress
- **Proof Export**: Export generated proofs to JSON format for sharing and verification
- **Interactive UI**: Modern interface built with Dear ImGui and OpenGL 3.3+

#### Build System
- **Root CMakeLists.txt**: Unified build system for libraries and GUI
- **Automatic Dependency Resolution**: Libraries are built automatically before GUI
- **Test Utility**: `test_circuit` CLI tool for testing ZK backend without GUI
- **Clean Script**: `clean.sh` for removing all generated build artifacts

#### Documentation
- **Comprehensive README.md** (516 lines):
  - Prerequisites and installation instructions (Fedora, Ubuntu, macOS)
  - Detailed build instructions
  - Usage guide with workflow examples
  - Architecture diagrams
  - Troubleshooting section
  - Known issues and limitations
  - FAQ with 10+ common questions
  - Performance metrics and benchmarks
  - Security considerations
  - Contributing guidelines

- **Quick Start Guide (QUICKSTART.md)** (163 lines):
  - 5-minute setup guide
  - Simple build instructions
  - First proof walkthrough
  - Performance reference table
  - Visual architecture diagram
  - Privacy comparison (traditional vs ZK)

- **Changelog (CHANGELOG.md)**: This file

#### Technical Implementation
- **Proof Generation Pipeline**:
  - ZK circuit compilation (~30-60 seconds)
  - Attribute selection and validation
  - Circuit caching metadata
  - Error handling and recovery
  
- **Proof Export Format** (JSON):
  - Version tracking
  - Timestamp
  - Proof hash
  - Circuit size
  - Attributes proven
  - Proof data (hex-encoded)
  - Generation settings

#### User Experience
- **Visual Feedback**:
  - Color-coded status messages
  - Real-time activity logging with timestamps
  - Tooltips with helpful information
  - Warning messages for long operations
  
- **Attribute Configuration**:
  - Age threshold slider (13-25 years)
  - Nationality input (ISO 3166-1 alpha-3)
  - Test document data configuration
  - Attribute selection checkboxes

### Technical Details

#### Dependencies
- **Graphics**: GLFW 3.4.0, GLEW 2.2.0, OpenGL 3.3+
- **GUI Framework**: Dear ImGui (git submodule)
- **Cryptography**: OpenSSL (with SHA256, AES-256)
- **Compression**: zstd
- **ZK Backend**: Longfellow ZK library (mdoc circuits)

#### Performance Metrics
- Circuit generation: 30-60 seconds (one-time operation)
- Proof creation: Instant (after circuit compilation)
- Proof verification: <1 second
- Memory usage: ~50-100 MB base, up to 500 MB during circuit generation
- Binary size: ~2.7 MB (GUI), ~1.1 MB (test utility)
- Circuit size: ~278 KB compressed (~88 MB uncompressed)

#### Build Artifacts
```
gui/
├── longfellow_gui       # Main GUI executable (2.7 MB)
├── test_circuit         # Test utility (1.1 MB)
├── CMakeLists.txt       # Build configuration
├── main.cpp             # Application source (458 lines)
├── test_circuit.cpp     # Test utility source (111 lines)
├── README.md            # Full documentation (516 lines)
├── QUICKSTART.md        # Quick start guide (163 lines)
├── CHANGELOG.md         # This file
├── clean.sh             # Cleanup script
└── imgui/               # ImGui library (git submodule)
```

### Changed
- **CMakeLists.txt**: Updated to use parent project's library targets
- **Build Process**: Now builds from project root instead of gui/ subdirectory

### Fixed
- **API Compatibility**: Corrected use of C API (removed incorrect `proofs::` namespace)
- **Error Handling**: Added comprehensive try-catch blocks and error messages
- **Type Conversions**: Fixed `uint8_t` to `std::string` conversions
- **Memory Management**: Proper `free()` calls for circuit data
- **Include Paths**: Corrected to use `CMAKE_SOURCE_DIR` for parent project references

### Security
- **Zero-Knowledge Proofs**: Personal data never leaves device
- **No Data Leakage**: Proofs reveal only explicitly chosen attributes
- **Cryptographic Guarantees**: Based on mdoc ISO 18013-5 standard
- **Demo Mode**: Uses test documents (production needs real mdoc integration)

### Known Issues
- **UI Freeze**: Interface freezes during 30-60 second circuit generation (synchronous operation)
- **No Cancellation**: Circuit generation cannot be cancelled once started
- **No Circuit Caching**: Circuits are regenerated for each proof (no disk caching)
- **OpenSSL Warnings**: Deprecation warnings during compilation (non-critical)
- **Single-threaded**: All operations run on main thread

### Future Improvements
- [ ] Async circuit generation with progress bar
- [ ] Circuit caching on disk
- [ ] Multi-threading for better responsiveness
- [ ] Real mdoc document integration
- [ ] Additional attribute types (document number, expiry date, etc.)
- [ ] Proof import functionality
- [ ] Headless mode for server deployment
- [ ] Dark/light theme toggle
- [ ] Internationalization (i18n)
- [ ] Network API for remote verification

## Development Notes

### Project Structure
```
longfellow-zk/
├── CMakeLists.txt           # Root build configuration (NEW)
├── .gitignore              # Updated with GUI artifacts
├── lib/                    # ZK proof libraries
│   ├── circuits/mdoc/      # mdoc circuit implementation
│   ├── algebra/            # Finite field arithmetic
│   ├── ec/                 # Elliptic curve operations
│   └── util/               # Cryptographic utilities
└── gui/                    # GUI application (NEW)
    ├── main.cpp
    ├── test_circuit.cpp
    ├── CMakeLists.txt
    ├── README.md
    ├── QUICKSTART.md
    ├── CHANGELOG.md
    ├── clean.sh
    └── imgui/              # External dependency
```

### Build Instructions Summary
```bash
# Setup
cd gui && git clone https://github.com/ocornut/imgui.git && cd ..

# Build
cmake .
make longfellow_gui

# Run
./gui/longfellow_gui

# Test without GUI
./gui/test_circuit

# Clean
./gui/clean.sh
```

### Git Integration
- **.gitignore** updated to exclude:
  - CMake generated files (CMakeCache.txt, Makefile, etc.)
  - Build artifacts (*.o, *.a, binaries)
  - Exported proofs (zkproof_*.json)
  - ImGui library (git submodule, managed separately)
  - Test executables

- **Files to commit**:
  - Source files: main.cpp, test_circuit.cpp
  - Build configs: CMakeLists.txt (root and gui/)
  - Documentation: README.md, QUICKSTART.md, CHANGELOG.md
  - Scripts: clean.sh
  - Configuration: .gitignore updates

## Testing

### Manual Testing Checklist
- [x] Application builds successfully from clean state
- [x] GUI launches and displays properly
- [x] Age verification proof generation works
- [x] Nationality verification proof generation works
- [x] Proof export to JSON works
- [x] Activity log displays all operations
- [x] Test utility runs without GUI
- [x] Clean script removes all generated files
- [x] Documentation is accurate and complete

### Platform Testing
- [x] Linux (Fedora) - Tested
- [ ] Linux (Ubuntu) - Not tested
- [ ] macOS - Not tested
- [ ] Windows - Not supported (GLFW/OpenGL required)

## License

Copyright 2025 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

## Contributors

- Initial GUI implementation and documentation

## References

- [Longfellow ZK Library](../lib/)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [ISO 18013-5 mDL Standard](https://www.iso.org/standard/69084.html)
- [Zero-Knowledge Proofs](https://en.wikipedia.org/wiki/Zero-knowledge_proof)

---

**Note**: This is the initial release. Future versions will address known issues and add requested features.