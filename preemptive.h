#include <algorithm>
#include <vector>
#include <cstdint>
#include "fast-slic-common.h"

class PreemptiveGrid {
private:
    bool enabled;
    int H;
    int W;
    int S;
    int CW;
    int CH;
    int assignment_memory_width;
    float thres;
    std::vector<int> num_changes;
    std::vector<bool> is_updatable;
    std::vector<bool> is_active;
    std::vector<uint16_t> old_assignment;
public:
    PreemptiveGrid(int H, int W, int S) : H(H), W(W), S(S) {
        CW = ceil_int(W, S);
        CH = ceil_int(H, S);
        num_changes.resize(CH * CW, 0);
        is_updatable.resize(CH * CW, true);
        is_active.resize(CH * CW, true);
    };

    void initialize(bool enabled, float thres) {
        this->enabled = enabled;
        this->thres = thres;
        std::fill(num_changes.begin(), num_changes.end(), 0);
        std::fill(is_updatable.begin(), is_updatable.end(), true);
        std::fill(is_active.begin(), is_active.end(), true);
    }

    void finalize() {
        std::fill(is_updatable.begin(), is_updatable.end(), true);
        std::fill(is_active.begin(), is_active.end(), true);
    }

    bool is_active_cluster(const Cluster& cluster) {
        return !enabled || is_active[CW * (cluster.y / S) + (cluster.x / S)];
    }

    bool is_updatable_cluster(const Cluster& cluster) {
        return !enabled || is_updatable[CW * (cluster.y / S) + (cluster.x / S)];
    }

    void set_old_assignment(const uint16_t* assignment, int memory_width) {
        old_assignment.resize(H * memory_width);
        std::copy(
            assignment,
            assignment + memory_width * H,
            this->old_assignment.begin()
        );
        std::fill(num_changes.begin(), num_changes.end(), 0);
    }

    void set_new_assignment(const uint16_t* assignment, int memory_width) {
        std::fill(num_changes.begin(), num_changes.end(), 0);

        #pragma omp parallel for
        for (int ci = 0; ci < CH; ci++) {
            int ei = my_min((ci + 1) * S, H);
            for (int i = ci * S; i < ei; i++) {
                for (int cj = 0; cj < CW; cj++) {
                    int cell_index = CW * ci + cj;
                    int ej = my_min((cj + 1) * S, W);
                    for (int j = cj * S; j < ej; j++) {
                        uint16_t old_label = old_assignment[memory_width * i + j];
                        uint16_t new_label = assignment[memory_width * i + j];
                        if (new_label != old_label)
                            num_changes[cell_index]++;
                    }
                }
            }
        }

        int thres_by_num_cells[10];
        for (int i = 0; i <= 9; i++) {
            thres_by_num_cells[i] = (int)(i * thres * S * S);
        }
        const int dir[3] = {-1, 0, 1};
        std::fill(is_active.begin(), is_active.end(), false);
        for (int ci = 0; ci < CH; ci++) {
            for (int cj = 0; cj < CW; cj++) {
                int num_cells = 0;
                int curr_num_changes = 0;
                for (int di : dir) {
                    int curr_ci = ci + di;
                    if (curr_ci < 0 || curr_ci >= CH) continue;
                    for (int dj : dir) {
                        int curr_cj = cj + dj;
                        if (curr_cj < 0 || curr_cj >= CW) continue;
                        num_cells++;
                        curr_num_changes += num_changes[CW * curr_ci + curr_cj];
                    }
                }
                bool updatable = curr_num_changes > thres_by_num_cells[num_cells];
                is_updatable[ci * CW + cj] = updatable;

                if (updatable) {
                    for (int di : dir) {
                        int curr_ci = ci + di;
                        if (curr_ci < 0 || curr_ci >= CH) continue;
                        for (int dj : dir) {
                            int curr_cj = cj + dj;
                            if (curr_cj < 0 || curr_cj >= CW) continue;
                            is_active[CW * curr_ci + curr_cj] = true;
                        }
                    }
                }
            }
        }
    }
};