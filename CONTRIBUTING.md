# Contributing to ESP32 Swiss Army Multitool

Thank you for your interest in contributing! This document provides guidelines for contributing to the project.

## Code of Conduct

- Be respectful and constructive
- Welcome newcomers and beginners
- Focus on what is best for the community
- Show empathy towards other community members

## How to Contribute

### Reporting Bugs

Before creating a bug report:
1. Check existing issues to avoid duplicates
2. Test with the latest version
3. Verify your hardware connections

Include in your bug report:
- **ESP32 Board:** Model and variant (e.g., "ESP32-WROOM-32")
- **Library Versions:** List all library versions
- **Serial Output:** Complete serial monitor output
- **Steps to Reproduce:** Detailed steps
- **Expected Behavior:** What should happen
- **Actual Behavior:** What actually happens
- **Hardware Setup:** Wiring diagram or description

### Suggesting Features

Feature requests should include:
- **Clear Description:** What feature do you want?
- **Use Case:** Why is this feature useful?
- **Implementation Ideas:** How might it work?
- **Hardware Requirements:** Any additional components needed?
- **Alternatives Considered:** Other ways to achieve the goal?

### Pull Requests

#### Before You Start

1. **Open an issue** to discuss major changes
2. **Check existing PRs** to avoid duplicates
3. **Fork the repository** and create a branch

#### Development Guidelines

**Code Style:**
- Use 2-space indentation
- Use descriptive variable names (no single letters except loop counters)
- Add comments for complex logic
- Use namespaces for related constants
- Follow existing code patterns

**ESP32-Specific Best Practices:**
- Avoid `String` class - use `char[]` arrays
- Use `F()` macro for string literals
- Protect shared variables with mutexes
- Keep ISRs short and use `IRAM_ATTR`
- Avoid GPIO strapping pins (0, 2, 5, 12, 15)
- Use `constrain()` for ADC values
- Always call `esp_task_wdt_reset()` in long-running tasks

**Memory Safety:**
- Check buffer sizes before `strcpy`/`sprintf`
- Use `strncpy`/`snprintf` with size limits
- Monitor heap usage during testing
- Avoid dynamic memory allocation in ISRs

**Thread Safety:**
- Use `xSemaphoreTake`/`xSemaphoreGive` for shared data
- Take mutexes with timeout, never `portMAX_DELAY` in ISRs
- Release mutexes in same function that acquired them
- Document which core/task accesses each variable

**Error Handling:**
- Check return values of critical functions
- Provide meaningful error messages via Serial
- Don't use `for(;;)` loops - allow graceful degradation
- Log errors but continue operation when possible

#### Testing Your Changes

Before submitting:
1. **Compile** without warnings
2. **Test** on actual hardware
3. **Monitor heap** usage (no leaks)
4. **Check serial output** for errors
5. **Test edge cases** (WiFi disconnect, display failure, etc.)
6. **Run for extended period** (30+ minutes)

#### Pull Request Process

1. **Update documentation** (README, CLAUDE.md if architecture changed)
2. **Write clear commit messages:**
   ```
   Short summary (50 chars or less)

   Detailed explanation of what changed and why.
   Reference issues: Fixes #123
   ```

3. **Create pull request:**
   - Title: Brief description
   - Description: What, why, and how
   - Link related issues
   - Include test results

4. **Respond to feedback:**
   - Address review comments
   - Make requested changes
   - Be open to suggestions

## Development Setup

### PlatformIO (Recommended)

```bash
git clone https://github.com/yourusername/esp32_swiss_army.git
cd esp32_swiss_army
pio run -t upload
pio device monitor
```

### Arduino IDE

1. Install ESP32 board support
2. Install required libraries (see README)
3. Open `esp32_swiss_army.ino`
4. Select board: ESP32 Dev Module
5. Configure partition scheme: Default or Minimal SPIFFS

## Project Structure

```
esp32_swiss_army/
├── esp32_swiss_army.ino    # Main sketch file
├── CLAUDE.md               # Architecture documentation for AI
├── README.md               # User documentation
├── CONTRIBUTING.md         # This file
├── LICENSE                 # MIT License
├── .gitignore             # Git ignore rules
└── platformio.ini         # PlatformIO configuration (optional)
```

## Code Organization

### Adding New Features

**New peripheral support:**
1. Add pin definition to `Pins` namespace
2. Add initialization in `setup()`
3. Add menu item and enum value
4. Implement case handler in `loop()`
5. Add web interface controls if needed

**New web endpoints:**
1. Add handler function with authentication check
2. Use POST for state-changing operations
3. Register in `wifiTask()` with `server.on()`
4. Update README with new API endpoints

**New configuration options:**
1. Add to appropriate namespace (Timing, WiFiConfig, TaskConfig, etc.)
2. Document in README
3. Use consistent naming conventions

## Architecture Guidelines

### Dual-Core Usage

- **Core 0:** Network operations only (WiFi, HTTP server)
- **Core 1:** Hardware control, display, sensors

### Mutex Usage

- **stateMutex:** Relay state, sensor values, WiFi info
- **i2cMutex:** Display operations, I2C sensors

Never hold multiple mutexes simultaneously (deadlock risk).

### Task Priorities

- WiFi task: Priority 2 (below lwIP at 18)
- Main loop: Priority 1 (Arduino default)

Don't set priorities above 17 (conflicts with network stack).

## Testing Checklist

For major changes:
- [ ] Compiles without warnings
- [ ] Tested on real hardware
- [ ] No memory leaks (heap stable over 1 hour)
- [ ] WiFi connection stable
- [ ] Display works correctly
- [ ] Serial output clean (no spam)
- [ ] Web interface functional
- [ ] Relay control works (local and remote)
- [ ] Handles WiFi disconnection gracefully
- [ ] Handles display failure gracefully
- [ ] Watchdog doesn't trigger
- [ ] Documentation updated

## Documentation

### Code Comments

Add comments for:
- Complex algorithms
- Hardware-specific requirements
- Timing-critical sections
- Non-obvious design decisions

Don't comment obvious code:
```cpp
// BAD
i++; // increment i

// GOOD
// Skip first sample to allow ADC to stabilize
i++;
```

### README Updates

Update README when:
- Adding/removing features
- Changing pin assignments
- Modifying WiFi behavior
- Altering web interface
- Changing dependencies

### CLAUDE.md Updates

Update CLAUDE.md when:
- Changing core architecture
- Modifying task structure
- Adding new build requirements
- Changing communication patterns

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

## Questions?

- Open an issue with the "question" label
- Check existing documentation first
- Be specific about what you need help with

## Recognition

Contributors will be acknowledged in:
- README Acknowledgments section
- Release notes
- Git commit history

Thank you for contributing to ESP32 Swiss Army Multitool!
