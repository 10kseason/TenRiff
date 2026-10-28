# Library, folder and difficulty-table loading

This local candidate inherits the complete 1.9.9.5 indicator/result candidate. Loading after selecting a chart is outside this change. The public version and fixed 1.9.6 baseline stay unchanged.

- A difficulty-table change now applies the new table to hashes already held in the loaded library. Previously it reread song caches and reloaded records, including replay verification. The importer still fetches updated content at the same URL. Single folders and ALL SONG use the same path.
- Fast-to-Safe changes, an empty library, running scans and completed unpublished scan results retain the normal reload path. F5 and folder refresh keep their validation. Table switching is no longer an implicit refresh of externally modified chart/record files; explicitly refresh when needed.
- Counting candidate files skips duplicate relative-path resolution, timestamp and size queries. The consuming pass retains path handling, permission-error behavior, cancellation, bounded batches and metadata validation.
- Parsed cache entries transfer ownership instead of being copied. Preserving the selected chart reads visible indices rather than copying every entry. Sorting, filtering and selection semantics remain.

No file formats, scoring, judgement, input, audio, replay/online contracts or models change. Use the separate library-loading-local profile; earlier releases, candidates and installations are preserved.

Measurements use 2,000 synthetic BMS charts/200 folders and a 10,000-entry cache, in-process HTTP responses and no records. OS file caches are not cleared. These are not measurements of the user's real library, cold-disk access, network latency, long sessions or other PCs. The candidate's evidence/VERIFICATION.ko.md and benchmark-comparison.json contain results and initial failures.

## Benchmark

Build the explicit `library_loading_benchmark` target, then run `build/Release/library_loading_benchmark.exe <new-output-directory>`. It refuses existing output paths and uses no real profile or network. The synthetic 10,000-entry cache includes copied metadata entries whose extra paths do not exist; only the first 2,000 charts are used to benchmark filesystem scans. This tool is excluded from default builds and end-user packages.

## Repeated-run medians

| Phase | Before (ms) | After (ms) | Reduction |
|---|---:|---:|---:|
| Folder scan (2,000 charts) | 1774.08 | 1320.67 | 25.6% |
| Open cached library (10,000 entries) | 233.99 | 230.95 | 1.3% |
| Apply table (10,000 entries) | 382.94 | 103.05 | 73.1% |
| Return to Native LV (10,000 entries) | 283.67 | 20.68 | 92.7% |

Three serial before/after pairs, reversing the second pair. Nine samples per version for scanning/opening and fifteen for table/native switching. The 1.3% cached-library opening difference is small and within run variation; no noticeable improvement is claimed there.
