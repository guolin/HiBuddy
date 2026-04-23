# pet_pack_builder

`pet_pack_builder` is a Python tool for generating Buddy pet pack outputs from a single input image.

It is intended for asset experimentation and pack export workflows, not for runtime use on the device.

## Install

```bash
python -m pip install -r tools/pet_pack_builder/requirements.txt
```

## Generate a Pack

```bash
python -m pet_pack_builder \
  --input ./base.png \
  --api-key "$OPENAI_API_KEY" \
  --api-base "$OPENAI_BASE_URL" \
  --model "${OPENAI_IMAGE_MODEL:-gpt-image-2}" \
  --out-dir ./output/chief-v1 \
  --pack-id chief-v1
```

## Mock Mode

For local pipeline validation without external API calls:

```bash
python -m pet_pack_builder \
  --input ./base.png \
  --out-dir ./output/mock-pack \
  --pack-id mock-pack \
  --mode mock
```

## Output Files

Typical outputs include:

- `manifest.json`
- `palette.json`
- `state_map.json`
- `frames_indexed.bin`
- `preview_sheet.png`
- `frames_preview.png`
- `palette_preview.png`
- `base_normalized.png`
- `prompts.json`
- `run_report.json`
- `raw_ai_frames/`
- `normalized_frames/`
- `reference_pack.h`

## Rebuild a Preview From an Existing Pack

```bash
python -m pet_pack_builder.preview \
  --pack-dir ./output/chief-v1 \
  --output ./output/chief-v1/reconstructed_preview.png
```

## Export a Reference C++ Header

```bash
python -m pet_pack_builder.export_cpp_header \
  --pack-dir ./output/chief-v1 \
  --output ./output/chief-v1/reference_pack_from_manifest.h
```

## Notes

- Keep API keys in environment variables, not in scripts or committed config files.
- Generated pack artifacts under `output/` are ignored by default.
