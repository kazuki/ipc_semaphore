# IPC Semaphore

プロセス間の同期に利用できるSemaphoreのPython/C++(pybind11)サンプル.

以下の実装がある(いずれもLinuxのみ対応)
* POSIX Semaphore: `ipc_semaphore.PosixSemaphore`
* 共有メモリに対するビジーウェイトを使ったSemaphore: `ipc_semaphore.BusyWaitSemaphore`
