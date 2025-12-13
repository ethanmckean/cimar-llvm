#!/bin/bash
# Unified CIMA Pipeline Script
# Supports base, nearest valid, and tainted pass variants with configurable options

set -e

# Directory where this script lives (tests/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Default values
PASS_VARIANT=""
CFG_MODE=""
DEBUG_FLAG=""
NEAREST_VALID_FLAG=""
KEEP_IR=false
OUTPUT_NAME=""
VALIDATE_MODE=false

# Show usage information
show_usage() {
    cat <<EOF
Usage: ./pipeline_unified.sh <source.c> --pass=VARIANT [OPTIONS]

Pass Selection (REQUIRED):
  --pass=base|nearest|tainted|all|asan|none    Select CIMA pass variant (required)
      base     - Base CIMA pass (returns undef values)
      nearest  - CIMA with nearest valid memory search
      tainted  - CIMA with dynamic taint tracking
      all      - Compile separately with each pass variant
      asan     - Skip CIMA pass (ASan only)
      none     - Skip both CIMA and ASan (raw compilation)

Configuration:
  --cfg[=all|final]              Generate CFG PDFs (default: final stage only)
  --debug                        Enable debug output (tainted pass only)
  --nearest-valid                Enable nearest-valid flag (nearest pass only)
  --validate                     Run validation tests (nearest pass only)
                                 Compares base vs nearest, verifies IR generation

Output:
  --keep-ir                      Keep intermediate .ll files
  --output=NAME                  Specify output binary name

Examples:
  ./pipeline_unified.sh test.c --pass=base
  ./pipeline_unified.sh test.c --pass=nearest --nearest-valid
  ./pipeline_unified.sh test.c --pass=nearest --nearest-valid --validate
  ./pipeline_unified.sh test.c --pass=tainted --debug
  ./pipeline_unified.sh test.c --pass=all --cfg=all
  ./pipeline_unified.sh test.c --pass=asan
  ./pipeline_unified.sh test.c --pass=none
EOF
}

# Parse arguments
INPUT_FILE=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --pass=*)
            PASS_VARIANT="${1#*=}"
            shift
            ;;
        --cfg)
            CFG_MODE="final"
            shift
            ;;
        --cfg=*)
            CFG_MODE="${1#*=}"
            shift
            ;;
        --debug)
            DEBUG_FLAG="-cima-debug"
            shift
            ;;
        --nearest-valid)
            NEAREST_VALID_FLAG="-cima-use-nearest-valid"
            shift
            ;;
        --keep-ir)
            KEEP_IR=true
            shift
            ;;
        --validate)
            VALIDATE_MODE=true
            shift
            ;;
        --output=*)
            OUTPUT_NAME="${1#*=}"
            shift
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        -*)
            echo "Error: Unknown option: $1"
            show_usage
            exit 1
            ;;
        *)
            if [ -z "$INPUT_FILE" ]; then
                INPUT_FILE="$1"
            else
                echo "Error: Multiple input files specified"
                show_usage
                exit 1
            fi
            shift
            ;;
    esac
done

# Validate input file
if [ -z "$INPUT_FILE" ]; then
    echo "Error: No input file specified"
    show_usage
    exit 1
fi

if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: Input file not found: $INPUT_FILE"
    exit 1
fi

# Validate --pass is provided
if [ -z "$PASS_VARIANT" ]; then
    echo "Error: --pass=VARIANT is required"
    show_usage
    exit 1
fi

# Validate pass variant
case $PASS_VARIANT in
    base|nearest|tainted|all|asan|none)
        ;;
    *)
        echo "Error: Invalid pass variant: $PASS_VARIANT"
        echo "Must be one of: base, nearest, tainted, all, asan, none"
        exit 1
        ;;
esac

# Validate CFG mode if specified
if [ -n "$CFG_MODE" ]; then
    case $CFG_MODE in
        final|all)
            ;;
        *)
            echo "Error: Invalid CFG mode: $CFG_MODE"
            echo "Must be one of: final, all"
            exit 1
            ;;
    esac
fi

# Auto-enable nearest-valid for nearest pass variant
if [ "$PASS_VARIANT" == "nearest" ]; then
    NEAREST_VALID_FLAG="-cima-use-nearest-valid"
fi

# Validate flag combinations
if [ "$PASS_VARIANT" != "tainted" ] && [ -n "$DEBUG_FLAG" ]; then
    echo "Warning: --debug flag only applies to tainted pass variant"
fi

if [ "$PASS_VARIANT" != "nearest" ] && [ -n "$NEAREST_VALID_FLAG" ]; then
    echo "Warning: --nearest-valid flag only applies to nearest pass variant"
fi

# Setup variables
BASENAME=$(basename "$INPUT_FILE" .c)
BUILD_DIR="../build/cimapass"
OUTPUT_DIR="build_tests"

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

# Function to generate CFG
generate_cfg() {
    local ll_file="$1"
    local stage_name="$2"
    local dot_name="$OUTPUT_DIR/${BASENAME}_${stage_name}.dot"
    local pdf_name="$OUTPUT_DIR/${BASENAME}_${stage_name}.pdf"

    echo "  Generating CFG for stage: $stage_name..."
    opt -passes='dot-cfg' \
        -disable-output \
        -cfg-func-name=main \
        "$ll_file" 2>/dev/null || true

    if [ -f ".main.dot" ]; then
        mv .main.dot "$dot_name"
        dot -Tpdf "$dot_name" -o "$pdf_name"
        echo "  CFG saved to: $pdf_name (dot: $dot_name)"
    fi
}

# Function to compile with a specific pass variant
compile_with_pass() {
    local variant="$1"
    local suffix="$2"

    echo ""
    echo "=== Compiling with pass variant: $variant ==="

    # Determine plugin, pass name, options, and runtime
    local PLUGIN=""
    local PASS_NAME=""
    local PASS_OPTS=""
    local RUNTIME_OBJ=""

    case $variant in
        base)
            PLUGIN="CIMAPass.so"
            PASS_NAME="CIMAPass"
            PASS_OPTS=""
            RUNTIME_OBJ=""
            ;;
        nearest)
            PLUGIN="CIMAPassNearestValid.so"
            PASS_NAME="CIMAPassNearestValid"
            PASS_OPTS="$NEAREST_VALID_FLAG"
            RUNTIME_OBJ="$BUILD_DIR/cima_runtime.o"
            if [ -n "$NEAREST_VALID_FLAG" ] && [ ! -f "$RUNTIME_OBJ" ]; then
                echo "Error: Runtime object not found: $RUNTIME_OBJ"
                echo "Please run ./build.sh first"
                exit 1
            fi
            ;;
        tainted)
            PLUGIN="CIMAPassTainted.so"
            PASS_NAME="CIMAPassTainted"
            PASS_OPTS="$DEBUG_FLAG"
            RUNTIME_OBJ=""
            ;;
        asan)
            PLUGIN=""
            PASS_NAME=""
            PASS_OPTS=""
            RUNTIME_OBJ=""
            ;;
        none)
            PLUGIN=""
            PASS_NAME=""
            PASS_OPTS=""
            RUNTIME_OBJ=""
            ;;
    esac

    # Output file names
    local RAW_LL="$OUTPUT_DIR/${BASENAME}.ll"
    local ASAN_LL="$OUTPUT_DIR/${BASENAME}${suffix}_asan.ll"
    local FINAL_LL="$OUTPUT_DIR/${BASENAME}${suffix}_final.ll"
    local BINARY="$OUTPUT_DIR/${BASENAME}${suffix}_final"

    if [ -n "$OUTPUT_NAME" ] && [ "$variant" != "all" ]; then
        BINARY="$OUTPUT_DIR/$OUTPUT_NAME"
    fi

    # Step 1: Compile C to LLVM IR (only if not already done for 'all' variant)
    # For 'none' variant, always regenerate raw IR without ASan
    if [ "$variant" == "none" ] || [ "$variant" == "all" ] || [ ! -f "$RAW_LL" ]; then
        if [ "$variant" == "none" ]; then
            echo "Step 1: Compiling C to LLVM IR (no instrumentation)..."
            clang -S -emit-llvm -O0 \
                -Xclang -disable-llvm-passes \
                -Xclang -disable-O0-optnone \
                "$INPUT_FILE" -o "$RAW_LL"
        else
            echo "Step 1: Compiling C to LLVM IR with ASan..."
            clang -S -emit-llvm -O0 \
                -fsanitize=address \
                -Xclang -disable-llvm-passes \
                -Xclang -disable-O0-optnone \
                "$INPUT_FILE" -o "$RAW_LL"
        fi

        if [ "$CFG_MODE" == "all" ]; then
            generate_cfg "$RAW_LL" "0_raw${suffix}"
        fi
    fi

    # Step 2: Run ASan pass (if not 'none')
    if [ "$variant" != "none" ]; then
        echo "Step 2: Running ASan pass..."
        opt -passes='module(asan),asan' \
            "$RAW_LL" -S -o "$ASAN_LL" 2>&1 | grep -v "Redundant instrumentation detected" || true

        if [ "$CFG_MODE" == "all" ]; then
            generate_cfg "$ASAN_LL" "1_asan${suffix}"
        fi
    else
        echo "Step 2: Skipping ASan pass (none variant)"
        cp "$RAW_LL" "$ASAN_LL"
    fi

    # Step 3: Run CIMA pass (if not 'none' or 'asan')
    if [ "$variant" != "none" ] && [ "$variant" != "asan" ]; then
        echo "Step 3: Running CIMA pass ($PASS_NAME)..."

        if [ ! -f "$BUILD_DIR/$PLUGIN" ]; then
            echo "Error: Plugin not found: $BUILD_DIR/$PLUGIN"
            echo "Please run ./build.sh first"
            exit 1
        fi

        opt -load-pass-plugin="$BUILD_DIR/$PLUGIN" \
            -passes="$PASS_NAME" \
            $PASS_OPTS \
            "$ASAN_LL" -S -o "$FINAL_LL" 2>&1 | grep -v "Redundant instrumentation detected" || true
    else
        echo "Step 3: Skipping CIMA pass ($variant variant)"
        cp "$ASAN_LL" "$FINAL_LL"
    fi

    if [ "$CFG_MODE" == "final" ] || [ "$CFG_MODE" == "all" ]; then
        generate_cfg "$FINAL_LL" "2_${variant}${suffix}"
    fi

    # Step 4: Link binary
    echo "Step 4: Linking binary..."
    if [ "$variant" == "none" ]; then
        clang "$FINAL_LL" -o "$BINARY" 2>&1 | grep -v "Redundant instrumentation detected" || true
    elif [ -n "$RUNTIME_OBJ" ] && [ -n "$NEAREST_VALID_FLAG" ]; then
        clang -fsanitize=address "$FINAL_LL" "$RUNTIME_OBJ" -o "$BINARY" 2>&1 | grep -v "Redundant instrumentation detected" || true
    else
        clang -fsanitize=address "$FINAL_LL" -o "$BINARY" 2>&1 | grep -v "Redundant instrumentation detected" || true
    fi

    echo "Binary created: $BINARY"

    # Clean up intermediate files if not keeping IR
    if [ "$KEEP_IR" = false ]; then
        rm -f "$ASAN_LL"
        if [ "$variant" == "all" ] || [ "$PASS_VARIANT" != "all" ]; then
            rm -f "$FINAL_LL"
        fi
    fi

    # Step 5: Run binary
    echo ""
    echo "--- Running binary: $BINARY ---"
    if [ "$variant" != "none" ]; then
        export ASAN_OPTIONS="detect_stack_use_after_return=0"
    fi
    ./"$BINARY" || true
    echo ""
}

# Validation function for nearest valid pass
run_validation() {
    echo ""
    echo "=== Running Validation Tests ==="
    echo ""

    # Ensure we have the raw IR and run ASan on it first
    local RAW_LL="$OUTPUT_DIR/${BASENAME}.ll"
    if [ ! -f "$RAW_LL" ]; then
        echo "Error: Raw IR not found. Validation requires IR files."
        exit 1
    fi

    # Generate ASan-instrumented IR for validation
    local VALIDATION_ASAN_LL="$OUTPUT_DIR/${BASENAME}_validation_asan.ll"
    opt -passes='module(asan),asan' \
        "$RAW_LL" -S -o "$VALIDATION_ASAN_LL" 2>&1 | grep -v "Redundant instrumentation detected" || true

    echo "1. Testing base CIMA pass (should use UndefValue)..."
    opt -load-pass-plugin="$BUILD_DIR/CIMAPass.so" \
        -passes='CIMAPass' \
        "$VALIDATION_ASAN_LL" -S -o "$OUTPUT_DIR/${BASENAME}_validation_base.ll" 2>&1 | grep -v "Redundant instrumentation detected" || true

    if grep -q "undef" "$OUTPUT_DIR/${BASENAME}_validation_base.ll"; then
        echo "   Base pass confirmed: using UndefValue"
    else
        echo "   Error: Base pass not working correctly"
        exit 1
    fi

    echo ""
    echo "2. Testing nearest valid CIMA pass (should generate helper calls)..."
    # Generate the nearest valid IR from the ASan IR
    opt -load-pass-plugin="$BUILD_DIR/CIMAPassNearestValid.so" \
        -passes='CIMAPassNearestValid' \
        -cima-use-nearest-valid \
        "$VALIDATION_ASAN_LL" -S -o "$OUTPUT_DIR/${BASENAME}_validation_nearest.ll" 2>&1 | grep -v "Redundant instrumentation detected" || true

    if grep -q "__cima_find_nearest_valid" "$OUTPUT_DIR/${BASENAME}_validation_nearest.ll"; then
        echo "   Nearest valid feature confirmed: helper calls generated"
    else
        echo "   Error: Helper calls not found in instrumented IR"
        exit 1
    fi

    # Cleanup
    rm -f "$OUTPUT_DIR/${BASENAME}_validation_nearest.ll"

    echo ""
    echo "3. Comparing runtime behavior..."
    echo "   Base pass output (undef values):"
    local BASE_BINARY="$OUTPUT_DIR/${BASENAME}_validation_base_final"
    clang -fsanitize=address "$OUTPUT_DIR/${BASENAME}_validation_base.ll" -o "$BASE_BINARY" 2>&1 | grep -v "Redundant instrumentation detected" || true
    export ASAN_OPTIONS="detect_stack_use_after_return=0"
    "$BASE_BINARY" 2>&1 | head -3 || true

    echo ""
    echo "   Nearest valid pass output (recovers from OOB):"
    ./"$OUTPUT_DIR/${BASENAME}_final" 2>&1 | head -3 || true

    # Cleanup validation artifacts
    rm -f "$OUTPUT_DIR/${BASENAME}_validation_asan.ll"
    rm -f "$OUTPUT_DIR/${BASENAME}_validation_base.ll"
    rm -f "$BASE_BINARY"

    echo ""
    echo "=== All validation tests passed ==="
    echo ""
}

# Main execution
echo "Processing $INPUT_FILE with pass variant: $PASS_VARIANT"

# Validate --validate flag usage
if [ "$VALIDATE_MODE" = true ] && [ "$PASS_VARIANT" != "nearest" ]; then
    echo "Error: --validate flag only works with --pass=nearest"
    exit 1
fi

if [ "$VALIDATE_MODE" = true ] && [ -z "$NEAREST_VALID_FLAG" ]; then
    echo "Error: --validate requires --nearest-valid flag"
    exit 1
fi

if [ "$PASS_VARIANT" == "all" ]; then
    # Compile with all variants
    # For 'all' variant, automatically enable nearest-valid for nearest pass
    NEAREST_VALID_FLAG="-cima-use-nearest-valid"

    compile_with_pass "base" "_base"
    compile_with_pass "nearest" "_nearest"
    compile_with_pass "tainted" "_tainted"
    compile_with_pass "asan" "_asan"
    compile_with_pass "none" "_none"

    # Clean up shared raw IR if not keeping
    if [ "$KEEP_IR" = false ]; then
        rm -f "$OUTPUT_DIR/${BASENAME}.ll"
    fi

    echo ""
    echo "=== All variants compiled successfully ==="
    echo "Binaries created:"
    echo "  - $OUTPUT_DIR/${BASENAME}_base_final"
    echo "  - $OUTPUT_DIR/${BASENAME}_nearest_final"
    echo "  - $OUTPUT_DIR/${BASENAME}_tainted_final"
    echo "  - $OUTPUT_DIR/${BASENAME}_asan_final"
    echo "  - $OUTPUT_DIR/${BASENAME}_none_final"
else
    # Compile with single variant
    SUFFIX=""
    if [ "$PASS_VARIANT" == "nearest" ]; then
        SUFFIX="_nearest"
    fi
    compile_with_pass "$PASS_VARIANT" "$SUFFIX"

    # Run validation if requested
    if [ "$VALIDATE_MODE" = true ]; then
        KEEP_IR=true  # Force keep IR for validation
        run_validation
    fi

    # Clean up raw IR if not keeping
    if [ "$KEEP_IR" = false ]; then
        rm -f "$OUTPUT_DIR/${BASENAME}.ll"
    fi
fi

echo ""
echo "=== Pipeline completed successfully ==="
