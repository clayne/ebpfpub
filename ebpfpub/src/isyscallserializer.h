#pragma once

#include "bpfprogramwriter.h"
#include "bufferreader.h"

#include <ebpfpub/isyscalltracepoint.h>

namespace ebpfpub {
class ISyscallSerializer {
public:
  using Ref = std::unique_ptr<ISyscallSerializer>;
  using Factory = StringErrorOr<Ref> (*)();

  ISyscallSerializer() = default;
  virtual ~ISyscallSerializer() = default;

  virtual const std::string &name() const = 0;

  virtual SuccessOrStringError generate(const ITracepointEvent &enter_event,
                                        BPFProgramWriter &bpf_prog_writer) = 0;

  virtual SuccessOrStringError parseEvents(ISyscallTracepoint::Event &event,
                                           BufferReader &buffer_reader,
                                           BufferStorage &buffer_storage) = 0;

  ISyscallSerializer(const ISyscallSerializer &) = delete;
  ISyscallSerializer &operator=(const ISyscallSerializer &) = delete;
};
} // namespace ebpfpub
