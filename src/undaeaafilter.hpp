// Anti-aliasing filters for common sample rates
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

#include "undaesos.hpp"

namespace undae {

    template <typename T>
    class AAFilter {
    public:
        void Init(float sampleRate) {
            InitFilter(sampleRate);
        }

        T ProcessUp(T in) {
            return upFilter_.Process(in);
        }

        T ProcessDown(T in) {
            return downFilter_.Process(in);
        }

        int GetOversamplingFactor(void) {
            return oversamplingFactor_;
        }

    protected:
        struct CascadedSOS {
            float sampleRate;
            int oversampling_factor;
            int num_sections;
            const SOSCoefficients* coeffs;
        };

        static const int kMaxNumSections = 7;

        SOSFilter<T, kMaxNumSections> upFilter_;
        SOSFilter<T, kMaxNumSections> downFilter_;
        int oversamplingFactor_;

        void InitFilter(float sampleRate) {
            if (false) {

            } else if (768000 <= sampleRate) {
                // n = 2, wc = 0.052083, cost = 768000
                const SOSCoefficients kFilter768000x1[1] = {
                    { {1.83197956e-02,  3.66063440e-02,  1.83197956e-02,  }, {-1.60702602e+00, 6.80271956e-01,  } },
                };
                upFilter_.Init(1, kFilter768000x1);
                downFilter_.Init(1, kFilter768000x1);
                oversamplingFactor_ = 1;
            } else if (705600 <= sampleRate) {
                // n = 2, wc = 0.056689, cost = 705600
                const SOSCoefficients kFilter705600x1[1] = {
                    { {2.13438638e-02,  4.26550556e-02,  2.13438638e-02,  }, {-1.57253460e+00, 6.57877382e-01,  } },
                };
                upFilter_.Init(1, kFilter705600x1);
                downFilter_.Init(1, kFilter705600x1);
                oversamplingFactor_ = 1;
            } else if (384000 <= sampleRate) {
                // n = 2, wc = 0.104167, cost = 384000
                const SOSCoefficients kFilter384000x1[1] = {
                    { {6.09620331e-02,  1.21896769e-01,  6.09620331e-02,  }, {-1.22760212e+00, 4.71422957e-01,  } },
                };
                upFilter_.Init(1, kFilter384000x1);
                downFilter_.Init(1, kFilter384000x1);
                oversamplingFactor_ = 1;
            } else if (352800 <= sampleRate) {
                // n = 2, wc = 0.113379, cost = 352800
                const SOSCoefficients kFilter352800x1[1] = {
                    { {6.99874107e-02,  1.39948456e-01,  6.99874107e-02,  }, {-1.16347041e+00, 4.43393682e-01,  } },
                };
                upFilter_.Init(1, kFilter352800x1);
                downFilter_.Init(1, kFilter352800x1);
                oversamplingFactor_ = 1;
            } else if (192000 <= sampleRate) {
                // n = 2, wc = 0.208333, cost = 192000
                const SOSCoefficients kFilter192000x1[1] = {
                    { {1.74603587e-01,  3.49188678e-01,  1.74603587e-01,  }, {-5.65216145e-01, 2.63611998e-01,  } },
                };
                upFilter_.Init(1, kFilter192000x1);
                downFilter_.Init(1, kFilter192000x1);
                oversamplingFactor_ = 1;
            } else if (176400 <= sampleRate) {
                // n = 2, wc = 0.226757, cost = 176400
                const SOSCoefficients kFilter176400x1[1] = {
                    { {1.95938020e-01,  3.91858763e-01,  1.95938020e-01,  }, {-4.62313019e-01, 2.46047822e-01,  } },
                };
                upFilter_.Init(1, kFilter176400x1);
                downFilter_.Init(1, kFilter176400x1);
                oversamplingFactor_ = 1;
            } else if (96000 <= sampleRate) {
                // n = 8, wc = 0.208333, cost = 768000
                const SOSCoefficients kFilter96000x2[4] = {
                    { {1.61637850e-04,  2.48564833e-04,  1.61637850e-04,  }, {-1.55379599e+00, 6.19242969e-01,  } },
                    { {1.00000000e+00,  -3.56106191e-03, 1.00000000e+00,  }, {-1.52397985e+00, 7.01779035e-01,  } },
                    { {1.00000000e+00,  -7.04269454e-01, 1.00000000e+00,  }, {-1.49925562e+00, 8.20191196e-01,  } },
                    { {1.00000000e+00,  -9.36222412e-01, 1.00000000e+00,  }, {-1.51854586e+00, 9.39911675e-01,  } },
                };
                upFilter_.Init(4, kFilter96000x2);
                downFilter_.Init(4, kFilter96000x2);
                oversamplingFactor_ = 2;
            } else if (88200 <= sampleRate) {
                // n = 8, wc = 0.226757, cost = 705600
                const SOSCoefficients kFilter88200x2[4] = {
                    { {2.14361684e-04,  3.44618768e-04,  2.14361684e-04,  }, {-1.51452462e+00, 5.91486912e-01,  } },
                    { {1.00000000e+00,  1.79381294e-01,  1.00000000e+00,  }, {-1.47183116e+00, 6.80568376e-01,  } },
                    { {1.00000000e+00,  -5.38705333e-01, 1.00000000e+00,  }, {-1.43146550e+00, 8.07687680e-01,  } },
                    { {1.00000000e+00,  -7.87002288e-01, 1.00000000e+00,  }, {-1.44140131e+00, 9.35689662e-01,  } },
                };
                upFilter_.Init(4, kFilter88200x2);
                downFilter_.Init(4, kFilter88200x2);
                oversamplingFactor_ = 2;
            } else if (48000 <= sampleRate) {
                // n = 12, wc = 0.277778, cost = 864000
                const SOSCoefficients kFilter48000x3[6] = {
                    { {1.96007199e-04,  3.15285921e-04,  1.96007199e-04,  }, {-1.49750952e+00, 5.79487424e-01,  } },
                    { {1.00000000e+00,  1.64502383e-01,  1.00000000e+00,  }, {-1.43900370e+00, 6.63196513e-01,  } },
                    { {1.00000000e+00,  -5.92180251e-01, 1.00000000e+00,  }, {-1.36241892e+00, 7.75058824e-01,  } },
                    { {1.00000000e+00,  -9.07488127e-01, 1.00000000e+00,  }, {-1.30223398e+00, 8.69165582e-01,  } },
                    { {1.00000000e+00,  -1.04177534e+00, 1.00000000e+00,  }, {-1.26951947e+00, 9.34679234e-01,  } },
                    { {1.00000000e+00,  -1.09276235e+00, 1.00000000e+00,  }, {-1.26454687e+00, 9.80322986e-01,  } },
                };
                upFilter_.Init(6, kFilter48000x3);
                downFilter_.Init(6, kFilter48000x3);
                oversamplingFactor_ = 3;
            } else if (44100 <= sampleRate) {
                // n = 14, wc = 0.302343, cost = 926100
                const SOSCoefficients kFilter44100x3[7] = {
                    { {2.33467524e-04,  3.85146244e-04,  2.33467524e-04,  }, {-1.46779940e+00, 5.59300587e-01,  } },
                    { {1.00000000e+00,  2.84344987e-01,  1.00000000e+00,  }, {-1.39743012e+00, 6.47280334e-01,  } },
                    { {1.00000000e+00,  -4.81735913e-01, 1.00000000e+00,  }, {-1.30466696e+00, 7.63828718e-01,  } },
                    { {1.00000000e+00,  -8.14458422e-01, 1.00000000e+00,  }, {-1.22921466e+00, 8.60153843e-01,  } },
                    { {1.00000000e+00,  -9.63424410e-01, 1.00000000e+00,  }, {-1.18164620e+00, 9.24279595e-01,  } },
                    { {1.00000000e+00,  -1.03102512e+00, 1.00000000e+00,  }, {-1.15782377e+00, 9.63657309e-01,  } },
                    { {1.00000000e+00,  -1.05757483e+00, 1.00000000e+00,  }, {-1.15253824e+00, 9.89272846e-01,  } },
                };
                upFilter_.Init(7, kFilter44100x3);
                downFilter_.Init(7, kFilter44100x3);
                oversamplingFactor_ = 3;
            } else if (24000 <= sampleRate) {
                // n = 8, wc = 0.333333, cost = 480000
                const SOSCoefficients kFilter24000x5[4] = {
                    { {9.93374792e-04,  1.81504524e-03,  9.93374792e-04,  }, {-1.28123502e+00, 4.43830055e-01,  } },
                    { {1.00000000e+00,  9.69736619e-01,  1.00000000e+00,  }, {-1.14056361e+00, 5.73274737e-01,  } },
                    { {1.00000000e+00,  3.23593812e-01,  1.00000000e+00,  }, {-9.84074266e-01, 7.48267989e-01,  } },
                    { {1.00000000e+00,  4.69137219e-02,  1.00000000e+00,  }, {-9.17508757e-01, 9.16260523e-01,  } },
                };
                upFilter_.Init(4, kFilter24000x5);
                downFilter_.Init(4, kFilter24000x5);
                oversamplingFactor_ = 5;
            } else if (22050 <= sampleRate) {
                // n = 8, wc = 0.302343, cost = 529200
                const SOSCoefficients kFilter22050x6[4] = {
                    { {6.47358611e-04,  1.15520581e-03,  6.47358611e-04,  }, {-1.35050917e+00, 4.84676642e-01,  } },
                    { {1.00000000e+00,  7.82770646e-01,  1.00000000e+00,  }, {-1.24212580e+00, 6.01760550e-01,  } },
                    { {1.00000000e+00,  9.46030879e-02,  1.00000000e+00,  }, {-1.12297856e+00, 7.63193697e-01,  } },
                    { {1.00000000e+00,  -1.84341946e-01, 1.00000000e+00,  }, {-1.08165394e+00, 9.20980215e-01,  } },
                };
                upFilter_.Init(4, kFilter22050x6);
                downFilter_.Init(4, kFilter22050x6);
                oversamplingFactor_ = 6;
            } else if (12000 <= sampleRate) {
                // n = 6, wc = 0.333333, cost = 360000
                const SOSCoefficients kFilter12000x10[3] = {
                    { {3.42306291e-03,  6.53522273e-03,  3.42306291e-03,  }, {-1.13209947e+00, 3.65774415e-01,  } },
                    { {1.00000000e+00,  1.42136933e+00,  1.00000000e+00,  }, {-9.55595652e-01, 5.55195466e-01,  } },
                    { {1.00000000e+00,  1.05842861e+00,  1.00000000e+00,  }, {-8.35474882e-01, 8.34840828e-01,  } },
                };
                upFilter_.Init(3, kFilter12000x10);
                downFilter_.Init(3, kFilter12000x10);
                oversamplingFactor_ = 10;
            } else if (11025 <= sampleRate) {
                // n = 6, wc = 0.329829, cost = 363825
                const SOSCoefficients kFilter11025x11[3] = {
                    { {3.26702718e-03,  6.22983576e-03,  3.26702718e-03,  }, {-1.14130758e+00, 3.70354990e-01,  } },
                    { {1.00000000e+00,  1.40863044e+00,  1.00000000e+00,  }, {-9.69538649e-01, 5.57917370e-01,  } },
                    { {1.00000000e+00,  1.03994151e+00,  1.00000000e+00,  }, {-8.54328717e-01, 8.35728285e-01,  } },
                };
                upFilter_.Init(3, kFilter11025x11);
                downFilter_.Init(3, kFilter11025x11);
                oversamplingFactor_ = 11;
            } else if (8000 <= sampleRate) {
                // n = 6, wc = 0.333333, cost = 360000
                const SOSCoefficients kFilter8000x15[3] = {
                    { {3.42306291e-03,  6.53522273e-03,  3.42306291e-03,  }, {-1.13209947e+00, 3.65774415e-01,  } },
                    { {1.00000000e+00,  1.42136933e+00,  1.00000000e+00,  }, {-9.55595652e-01, 5.55195466e-01,  } },
                    { {1.00000000e+00,  1.05842861e+00,  1.00000000e+00,  }, {-8.35474882e-01, 8.34840828e-01,  } },
                };
                upFilter_.Init(3, kFilter8000x15);
                downFilter_.Init(3, kFilter8000x15);
                oversamplingFactor_ = 15;
            } else {
                InitFilter(8000);
            }
        }
    };

}
