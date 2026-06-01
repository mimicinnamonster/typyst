#!/bin/sh
# typyst rendering diagnostics
# run this in typyst and look for misalignment

echo "=== Test 1: Simple numbered list (no special chars) ==="
echo "1. abcdefghijklmnopqrstuvwxyz"
echo "2. abcdefghijklmnopqrstuvwxyz"
echo "3. abcdefghijklmnopqrstuvwxyz"
echo ""

echo "=== Test 2: With tabs (like pi might send) ==="
printf "1.\tClear Safari caches\n"
printf "2.\tDisable hardware acceleration\n"
printf "3.\tCheck Safari extensions\n"
echo ""

echo "=== Test 3: With emoji (wide chars in list) ==="
echo "1. 🔬 Safari -> Develop -> Empty Caches"
echo "2. 🧩 Check Safari Extensions"
echo "3. 🖼️ Accessibility -> Reduce Transparency"
echo ""

echo "=== Test 4: With **bold** markers ==="
echo "1. **Clear Safari caches** - Safari -> Develop"
echo "2. **Disable hardware acceleration** - Develop menu"
echo "3. **Check Safari extensions** - Settings"
echo ""

echo "=== Test 5: Mixed (actual text from pi session) ==="
echo "1. **Clear Safari caches** - Safari -> Develop -> Empty Caches"
echo "2. **Clear all website data** - Safari -> Settings -> Privacy"
echo "3. **Disable Safari extensions**"
echo "4. **Try Chrome or Firefox**"
echo ""

echo "=== Test 6: Just tabs vs spaces ==="
printf "1.\tTab item\n"
printf "12.\tTab item\n"
printf "123.\tTab item\n"
echo "1. Space item"
echo "12. Space item"
echo "123. Space item"
echo ""

echo "=== Test 7: Arrow chars (→) ==="
echo "1. Safari -> Settings -> Advanced"
echo "2. System Settings -> Displays"
echo "3. Develop -> Experimental Features -> GPU Process"
echo ""

echo "=== Test 8: Wide chars only ==="
echo "🟢 🟡 🔴 Safari"
echo "🔬 🧩 🖼️ Chrome"
echo "✅ ❌ 🚀 Firefox"
echo ""

echo "=== Test 9: Baseline - ruler ==="
echo "0....+....1....+....2....+....3....+....4....+....5"
echo "1. Text aligns here -----^"
printf "1.\tTab aligns here --------^\n"
echo ""

echo "=== All tests complete ==="
echo ""
echo "Do any of these look misaligned in typyst?"
echo "If so, which test number(s) and what does the misalignment look like?"
