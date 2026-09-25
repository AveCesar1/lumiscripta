# Lumiscripta Documentation

Technical documentation for the Lumiscripta codebase. Everything here describes the
**current implementation** as found in `include/`, `src/`, and the `Makefile`. Where
older prose (e.g. parts of the root `README.md`) disagrees with the code, the code wins.

## Structure

```
docs/
├── README.md                  <- this index
├── architecture/              <- design documents for the four-unit architecture
│   ├── overview.md
│   ├── class-diagram.md
│   ├── application-flow.md
│   ├── rendering-pipeline.md
│   ├── image-pipeline.md
│   ├── font-system.md
│   └── decisions.md
└── screenshots/               <- images referenced from the root README.md
```

## Index

| Document | Contents |
|---|---|
| [Documentation index](README.md) | This file — structure and complete list of documents |
| [Architecture overview](architecture/overview.md) | The four units (`App`, `Graphics`, `File`, `Utils`), ownership, dependencies, design philosophy |
| [Class diagram](architecture/class-diagram.md) | Mermaid diagram of the principal classes, relationships, and public interfaces |
| [Application flow](architecture/application-flow.md) | Startup, initialization, main loop, view-mode dispatch, shutdown |
| [Rendering pipeline](architecture/rendering-pipeline.md) | How state and Markdown become ImGui/OpenGL output: editor and preview paths, `md4c`, `imgui_md` |
| [Image pipeline](architecture/image-pipeline.md) | Local image resolution, caching, `stb_image` decoding, GL textures, failure handling, cleanup |
| [Font system](architecture/font-system.md) | Merged font atlases, FreeType, colour emoji, glyph ranges, load-bearing details |
| [Architectural decisions](architecture/decisions.md) | Verified design decisions and the rationale visible in the code |
