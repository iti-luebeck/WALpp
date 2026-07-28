// test_writer_roundtrip.cpp - the property the writer tests never checked.
//
// test_vcd_writer.cpp asserts that the output *contains* strings like "$timescale" and
// "#0". A file can satisfy all of that and still be unreadable -- which is exactly what
// both writers produced: the VCD had no $enddefinitions (so readers found zero signals)
// and the FST was ~80 bytes of non-FST. Substring checks cannot catch that.
//
// So this test writes a trace and READS IT BACK with WAL's own parser, asserting on the
// decoded values. If a writer regresses, this fails; a file that merely looks plausible
// does not pass.

#include "wal/fast/vcd_parser.h"
#include "wal/writers/fst_writer.h"
#include "wal/writers/vcd_writer.h"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

struct Sample {
    uint64_t time;
    const char* clk;   // 1 bit
    const char* data;  // 8 bits
    const char* addr;  // 32 bits
};

const std::vector<Sample> kSamples = {
    {0, "0", "00000000", "00000000000000000000000000000000"},
    {10, "1", "10101010", "01000000000000000000000000000000"},
    {20, "0", "11111111", "01000000000000000000000000000100"},
};

int failures = 0;

void check(bool cond, const std::string& what) {
    if (!cond) {
        std::cerr << "  FAIL: " << what << std::endl;
        ++failures;
    }
}

template <typename Writer>
void write_trace(const std::string& path) {
    Writer w(path);
    w.set_time_scale("ns");
    w.add_signal("clk", 1);
    w.add_signal("data", 8);
    w.add_signal("addr", 32);
    for (const auto& s : kSamples) {
        w.write_value("clk", s.time, s.clk);
        w.write_value("data", s.time, s.data);
        w.write_value("addr", s.time, s.addr);
    }
    w.finalize();
}

/// Decode with WAL's own parser and rebuild the full state at every timestamp.
struct Decoded {
    std::vector<int64_t> times;
    std::vector<std::map<std::string, int64_t>> state;
    std::map<std::string, int> width;
};

Decoded decode(const std::string& path, bool is_fst) {
    fast_vcd::VCDParser p;
    if (is_fst) {
        p.parse_fst(path);
    } else {
        p.parse(path);
    }
    Decoded d;
    for (auto id : p.get_signal_ids()) {
        d.width[p.get_signal_info(id).name] = p.get_signal_info(id).width;
    }
    std::map<std::string, int64_t> cur;
    for (const auto& delta : p.get_deltas()) {
        for (const auto& c : delta.changes) {
            cur[p.get_signal_info(c.first).name] = c.second;
        }
        d.times.push_back(delta.timestamp);
        d.state.push_back(cur);
    }
    return d;
}

int64_t at(const Decoded& d, size_t idx, const std::string& suffix) {
    for (const auto& [name, value] : d.state.at(idx)) {
        if (name.size() >= suffix.size() &&
            name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            return value;
        }
    }
    return -1;
}

int width_of(const Decoded& d, const std::string& suffix) {
    for (const auto& [name, w] : d.width) {
        if (name.size() >= suffix.size() &&
            name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            return w;
        }
    }
    return -1;
}

void verify(const Decoded& d, const std::string& label) {
    std::cout << "  " << label << ": " << d.times.size() << " timestamps, " << d.width.size()
              << " signals" << std::endl;

    check(d.width.size() == 3, label + ": all three signals are declared");
    check(width_of(d, "clk") == 1, label + ": clk is 1 bit");
    check(width_of(d, "data") == 8, label + ": data is 8 bits");
    check(width_of(d, "addr") == 32, label + ": addr is 32 bits");

    // Every timestamp must survive as its own index -- a writer that loses the time
    // markers collapses the trace into a single point.
    check(d.times.size() == kSamples.size(), label + ": every timestamp round-trips");
    if (d.times.size() != kSamples.size()) {
        return;
    }
    for (size_t i = 0; i < kSamples.size(); ++i) {
        check(d.times[i] == static_cast<int64_t>(kSamples[i].time),
              label + ": timestamp " + std::to_string(i) + " preserved");
    }

    // And the values must decode to what was written.
    check(at(d, 0, "addr") == 0x00000000, label + ": addr at t=0");
    check(at(d, 1, "addr") == 0x40000000, label + ": addr at t=10");
    check(at(d, 2, "addr") == 0x40000004, label + ": addr at t=20");
    check(at(d, 1, "data") == 0xAA, label + ": data at t=10");
    check(at(d, 2, "data") == 0xFF, label + ": data at t=20");
    check(at(d, 1, "clk") == 1, label + ": clk high at t=10");
    check(at(d, 2, "clk") == 0, label + ": clk low at t=20");
}

void test_hierarchical_names() {
    // A $var must carry the LEAF name; the enclosing $scope supplies the path. Declaring
    // the full dotted name yields "top.cpu.top.cpu.pc" once a reader reassembles it.
    const std::string path = "roundtrip_hier.vcd";
    {
        wal::writers::VCDWriter w(path);
        w.set_time_scale("ns");
        w.add_hierarchical_signal("top.cpu", "pc", 32);
        w.write_value("top.cpu.pc", 0, "01000000000000000000000000000000");
        w.finalize();
    }
    fast_vcd::VCDParser p;
    p.parse(path);
    bool found = false;
    for (auto id : p.get_signal_ids()) {
        const auto& info = p.get_signal_info(id);
        std::cout << "  hierarchical signal: " << info.name << std::endl;
        if (info.name.find("pc") != std::string::npos &&
            info.name.find("pc.pc") == std::string::npos &&
            info.name.find("cpu.top") == std::string::npos) {
            found = true;
        }
    }
    check(found, "hierarchical signal keeps a sane full name");
    std::remove(path.c_str());
}

} // namespace

int main() {
    std::cout << "=== writer round-trip (write -> read back with WAL's own parser) ===" << std::endl;

    const std::string vcd = "roundtrip.vcd";
    write_trace<wal::writers::VCDWriter>(vcd);
    verify(decode(vcd, /*is_fst=*/false), "VCD");
    std::remove(vcd.c_str());

    const std::string fst = "roundtrip.fst";
    write_trace<wal::writers::FSTWriter>(fst);
    verify(decode(fst, /*is_fst=*/true), "FST");
    std::remove(fst.c_str());

    test_hierarchical_names();

    if (failures != 0) {
        std::cerr << failures << " check(s) FAILED" << std::endl;
        return 1;
    }
    std::cout << "all round-trip checks passed" << std::endl;
    return 0;
}
