# Quick Start Guide - Longfellow ZK GUI

Get started with the zero-knowledge identity verification GUI in 5 minutes.

## Prerequisites

Install dependencies (one command):

```bash
# Fedora/RHEL
sudo dnf install cmake gcc-c++ openssl-devel zstd-devel glfw-devel glew-devel mesa-libGL-devel

# Ubuntu/Debian
sudo apt install cmake g++ libssl-dev libzstd-dev libglfw3-dev libglew-dev libgl1-mesa-dev

# macOS
brew install cmake openssl zstd glfw glew
```

## Build (3 steps)

```bash
# 1. Clone ImGui library
cd gui
git clone https://github.com/ocornut/imgui.git
cd ..

# 2. Build everything
cmake .
make longfellow_gui

# 3. Run the application
./gui/longfellow_gui
```

**Build time**: ~2 minutes on a modern CPU

## First Proof (30 seconds)

1. ✅ Check "Age Verification" (default: 18 years old)
2. 🔐 Click "Generate ZK Proof"
3. ⏳ **Wait 30-60 seconds** (UI will freeze - this is normal!)
4. ✓ Click "Verify Proof"
5. 🎉 Done! You just created a zero-knowledge proof!

## What Just Happened?

You proved you're over 18 **without revealing**:
- Your actual age
- Your birth date
- Any personal information

The verifier only learns: "This person is ≥ 18 years old" ✅

## Important Notes

⚠️ **UI Freeze**: The interface freezes for 30-60 seconds during proof generation. This is **normal** - it's compiling cryptographic circuits. Be patient!

⚠️ **First Time**: Circuit generation is a one-time cryptographic compilation process. It's CPU-intensive but ensures privacy.

## Testing Without GUI

If you want to test the backend without the graphical interface:

```bash
make test_circuit
./gui/test_circuit
```

This runs a command-line test that:
- Lists available ZK specifications
- Generates a test circuit (~40 seconds)
- Validates the cryptographic operations
- Exits automatically

## Troubleshooting

### "Cannot find imgui/imgui.cpp"
```bash
cd gui && git clone https://github.com/ocornut/imgui.git && cd ..
```

### "undefined reference to 'generate_circuit'"
Build from project root, not from `gui/`:
```bash
cd /path/to/longfellow-zk
cmake .
make longfellow_gui
```

### Application crashes or freezes
1. Test the backend first: `./gui/test_circuit`
2. Check available RAM (needs ~500 MB)
3. Run from terminal to see error messages
4. Check the Activity Log in the GUI

### GUI doesn't appear
Ensure you have:
- OpenGL 3.3+ support
- Proper graphics drivers
- X11 or Wayland display server running

## Next Steps

- 📖 Read the full [README.md](README.md) for detailed documentation
- 🔧 Explore different attribute combinations
- 🎯 Try nationality verification
- 💻 Examine the code in `main.cpp`
- 📚 Learn about the Longfellow ZK library in `../docs/`

## Performance Reference

| Operation | Time | Memory |
|-----------|------|--------|
| Circuit Generation | 30-60s | ~500 MB |
| Proof Creation | Instant | ~100 MB |
| Proof Verification | <1s | ~50 MB |
| Total First Proof | ~40s | ~500 MB |

## Architecture Overview

```
┌─────────────────────────────────────┐
│   ImGui GUI (OpenGL + GLFW)         │  ← What you see
├─────────────────────────────────────┤
│   main.cpp (UI Logic)               │  ← Application code
├─────────────────────────────────────┤
│   Longfellow ZK Library             │  ← Zero-knowledge proofs
│   - Circuit compilation             │
│   - Proof generation                │
│   - Proof verification              │
└─────────────────────────────────────┘
```

## What Makes This Private?

**Traditional Identity Verification:**
```
You → [Send Birth Date: 1990-05-15] → Verifier
Verifier sees: "Born 1990-05-15, therefore 34 years old ✓"
❌ Verifier learns your exact birth date
```

**Zero-Knowledge Identity Verification:**
```
You → [Send ZK Proof: 0x4a9b2c...] → Verifier
Verifier sees: "Proof valid, age ≥ 18 ✓"
✅ Verifier learns ONLY that you're ≥ 18
✅ Your birth date remains private
✅ Cryptographically guaranteed
```

## Support

- 💬 Open an issue on the project repository
- 📖 Check [README.md](README.md) for detailed docs
- 🐛 See "Known Issues" section in README
- 📧 Contact the development team

---

**Built with ❤️ for privacy-preserving identity verification**

*Zero-Knowledge Proofs: Prove something is true without revealing why it's true.*