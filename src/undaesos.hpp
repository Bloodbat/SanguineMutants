// Cascaded second-order sections IIR filter
// Copyright (C) 2020 Tyler Coy
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

namespace undae {
    struct SOSCoefficients {
        float b[3];
        float a[2];
    };

    template <typename T, int maxSections>
    class SOSFilter {
    public:
        SOSFilter() {
            Init(0);
        }

        SOSFilter(int sectionCount) {
            Init(sectionCount);
        }

        void Init(int sectionCount) {
            sectionCount_ = sectionCount;
            Reset();
        }

        void Init(int sectionCount, const SOSCoefficients* sections) {
            sectionCount_ = sectionCount;
            Reset();
            SetCoefficients(sections);
        }

        void Reset() {
            for (int section = 0; section < sectionCount_; ++section) {
                x_[section][0] = 0.f;
                x_[section][1] = 0.f;
                x_[section][2] = 0.f;
            }

            x_[sectionCount_][0] = 0.f;
            x_[sectionCount_][1] = 0.f;
            x_[sectionCount_][2] = 0.f;
        }

        void SetCoefficients(const SOSCoefficients* sections) {
            for (int section = 0; section < sectionCount_; ++section) {
                sections_[section].b[0] = sections[section].b[0];
                sections_[section].b[1] = sections[section].b[1];
                sections_[section].b[2] = sections[section].b[2];

                sections_[section].a[0] = sections[section].a[0];
                sections_[section].a[1] = sections[section].a[1];
            }
        }

        T Process(T in) {
            for (int section = 0; section < sectionCount_; ++section) {
                // Shift x state
                x_[section][2] = x_[section][1];
                x_[section][1] = x_[section][0];
                x_[section][0] = in;

                T out = 0.f;

                // Add x state
                out += sections_[section].b[0] * x_[section][0];
                out += sections_[section].b[1] * x_[section][1];
                out += sections_[section].b[2] * x_[section][2];

                // Subtract y state
                out -= sections_[section].a[0] * x_[section + 1][0];
                out -= sections_[section].a[1] * x_[section + 1][1];
                in = out;
            }

            // Shift final section x state
            x_[sectionCount_][2] = x_[sectionCount_][1];
            x_[sectionCount_][1] = x_[sectionCount_][0];
            x_[sectionCount_][0] = in;

            return in;
        }

    protected:
        int sectionCount_;
        SOSCoefficients sections_[maxSections];
        T x_[maxSections + 1][3];
    };

}
