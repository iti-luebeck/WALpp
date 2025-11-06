#pragma once

#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <vector>
#include <memory>

namespace wal::operators {

/// VisualizationOperators - Plotting and visualization utilities
/// 
/// Operators: ascii-plot, gnuplot-script, export-tsv, html-plot
class VisualizationOperators {
public:
    static void register_all(OperatorRegistry& registry) {
        // ═══════════════════════════════════════════════════════════════
        // VISUALIZATION & EXPORT (4 operators)
        // ═══════════════════════════════════════════════════════════════

        // ascii-plot: Create ASCII art plot of signal
        registry.register_operator("ascii-plot", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() < 1 || args.size() > 2) throw EvalError("ascii-plot expects 1 or 2 arguments");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("ascii-plot requires a list argument");
            }
            
            auto list = signals[0].to_list();
            int height = 10;
            
            if (args.size() == 2) {
                if (!signals[1].is_int()) {
                    throw EvalError("ascii-plot height must be an integer");
                }
                height = signals[1].to_int();
            }
            
            if (list->empty()) throw EvalError("ascii-plot: empty signal");
            
            std::vector<double> values;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    values.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    values.push_back(v.to_float());
                } else {
                    throw EvalError("ascii-plot: all elements must be numeric");
                }
            }
            
            double min_val = *std::min_element(values.begin(), values.end());
            double max_val = *std::max_element(values.begin(), values.end());
            double range = max_val - min_val;
            
            if (range < 1e-10) range = 1.0;
            
            std::ostringstream oss;
            oss << "\nSignal Plot (height=" << height << "):\n";
            oss << "┌";
            for (size_t i = 0; i < std::min(values.size(), size_t(60)); ++i) oss << "─";
            oss << "┐\n";
            
            for (int row = height - 1; row >= 0; --row) {
                double threshold_high = min_val + (row + 1) * range / height;
                double threshold_low = min_val + row * range / height;
                
                oss << "│";
                for (size_t i = 0; i < std::min(values.size(), size_t(60)); ++i) {
                    if (values[i] >= threshold_low && values[i] < threshold_high) {
                        oss << "█";
                    } else {
                        oss << " ";
                    }
                }
                oss << "│\n";
            }
            
            oss << "└";
            for (size_t i = 0; i < std::min(values.size(), size_t(60)); ++i) oss << "─";
            oss << "┘\n";
            
            return Value(oss.str());
        });

        // gnuplot-script: Generate gnuplot script for plotting
        registry.register_operator("gnuplot-script", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() < 2 || args.size() > 3) throw EvalError("gnuplot-script expects 2-3 arguments");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("gnuplot-script: first argument must be a list");
            }
            if (!signals[1].is_string()) {
                throw EvalError("gnuplot-script: second argument must be a string (title)");
            }
            
            auto list = signals[0].to_list();
            const auto& title = signals[1].to_string();
            std::string output_file = "plot.png";
            
            if (args.size() == 3) {
                if (!signals[2].is_string()) {
                    throw EvalError("gnuplot-script: output file must be a string");
                }
                output_file = signals[2].to_string();
            }
            
            std::ostringstream oss;
            oss << "#!/usr/bin/gnuplot\n";
            oss << "set terminal png size 800,600\n";
            oss << "set output '" << output_file << "'\n";
            oss << "set title '" << title << "'\n";
            oss << "set xlabel 'Sample'\n";
            oss << "set ylabel 'Value'\n";
            oss << "set grid\n";
            oss << "plot '-' using 1:2 with linespoints title 'Signal'\n";
            
            for (size_t i = 0; i < list->size(); ++i) {
                const auto& v = (*list)[i];
                double val = 0;
                if (v.is_int()) {
                    val = static_cast<double>(v.to_int());
                } else if (v.is_float()) {
                    val = v.to_float();
                } else {
                    throw EvalError("gnuplot-script: all elements must be numeric");
                }
                oss << i << " " << val << "\n";
            }
            oss << "e\n";
            
            return Value(oss.str());
        });

        // export-tsv: Export signal to tab-separated values
        registry.register_operator("export-tsv", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("export-tsv expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("export-tsv requires a list argument");
            }
            
            auto list = signals[0].to_list();
            std::ostringstream oss;
            
            oss << "Index\tValue\n";
            for (size_t i = 0; i < list->size(); ++i) {
                const auto& v = (*list)[i];
                oss << i << "\t";
                
                if (v.is_int()) {
                    oss << v.to_int();
                } else if (v.is_float()) {
                    oss << std::fixed << std::setprecision(6) << v.to_float();
                } else if (v.is_string()) {
                    oss << v.to_string();
                } else {
                    oss << v.to_string_repr();
                }
                oss << "\n";
            }
            
            return Value(oss.str());
        });

        // html-plot: Generate simple HTML/SVG plot
        registry.register_operator("html-plot", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() < 1 || args.size() > 2) throw EvalError("html-plot expects 1 or 2 arguments");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("html-plot requires a list argument");
            }
            
            auto list = signals[0].to_list();
            const auto& title = (args.size() == 2 && signals[1].is_string()) ? signals[1].to_string() : "Signal Plot";
            
            if (list->empty()) throw EvalError("html-plot: empty signal");
            
            std::vector<double> values;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    values.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    values.push_back(v.to_float());
                } else {
                    throw EvalError("html-plot: all elements must be numeric");
                }
            }
            
            double min_val = *std::min_element(values.begin(), values.end());
            double max_val = *std::max_element(values.begin(), values.end());
            double range = max_val - min_val;
            if (range < 1e-10) range = 1.0;
            
            const int svg_width = 800;
            const int svg_height = 400;
            const int padding = 50;
            
            std::ostringstream oss;
            oss << "<!DOCTYPE html>\n<html>\n<head>\n";
            oss << "  <title>" << title << "</title>\n";
            oss << "  <style>body { font-family: Arial; } svg { border: 1px solid #ccc; }</style>\n";
            oss << "</head>\n<body>\n";
            oss << "  <h1>" << title << "</h1>\n";
            oss << "  <svg width=\"" << svg_width << "\" height=\"" << svg_height << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";
            
            // Grid
            oss << "    <defs><pattern id=\"grid\" width=\"40\" height=\"40\" patternUnits=\"userSpaceOnUse\">";
            oss << "<path d=\"M 40 0 L 0 0 0 40\" fill=\"none\" stroke=\"#e0e0e0\" stroke-width=\"0.5\"/></pattern></defs>\n";
            oss << "    <rect width=\"" << svg_width << "\" height=\"" << svg_height << "\" fill=\"url(#grid)\"/>\n";
            
            // Plot area border
            oss << "    <rect x=\"" << padding << "\" y=\"" << padding << "\" ";
            oss << "width=\"" << (svg_width - 2*padding) << "\" height=\"" << (svg_height - 2*padding) << "\" ";
            oss << "fill=\"none\" stroke=\"black\" stroke-width=\"2\"/>\n";
            
            // Polyline path for signal
            oss << "    <polyline points=\"";
            for (size_t i = 0; i < values.size(); ++i) {
                double x = padding + (i * (svg_width - 2.0*padding)) / (values.size() - 1);
                double y = svg_height - padding - ((values[i] - min_val) / range) * (svg_height - 2.0*padding);
                oss << x << "," << y << " ";
            }
            oss << "\" fill=\"none\" stroke=\"blue\" stroke-width=\"2\"/>\n";
            
            // Axes labels
            oss << "    <text x=\"" << (svg_width/2) << "\" y=\"" << (svg_height - 10) << "\" text-anchor=\"middle\">Index</text>\n";
            oss << "    <text x=\"20\" y=\"" << (svg_height/2) << "\" text-anchor=\"middle\" transform=\"rotate(-90 20 " << (svg_height/2) << ")\">Value</text>\n";
            
            oss << "  </svg>\n";
            oss << "</body>\n</html>\n";
            
            return Value(oss.str());
        });
    }
};

}  // namespace wal::operators
