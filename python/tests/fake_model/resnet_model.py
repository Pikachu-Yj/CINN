# Copyright (c) 2021 CINN Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import numpy
import paddle
import sys, os
import numpy as np
import paddle.fluid as fluid
from paddle.fluid.backward import append_backward

paddle.enable_static()

resnet_input = fluid.layers.data(
    name="resnet_input",
    append_batch_size=False,
    shape=[1, 160, 7, 7],
    dtype='float32')
label = fluid.layers.data(
    name="label",
    append_batch_size=False,
    shape=[1, 960, 7, 7],
    dtype='float32')
d = fluid.layers.relu6(resnet_input)
f = fluid.layers.conv2d(
    input=d, num_filters=960, filter_size=1, stride=1, padding=0, dilation=1)
g = fluid.layers.conv2d(
    input=f, num_filters=160, filter_size=1, stride=1, padding=0, dilation=1)
i = fluid.layers.conv2d(
    input=g, num_filters=960, filter_size=1, stride=1, padding=0, dilation=1)
j1 = fluid.layers.scale(i, scale=2.0, bias=0.5)
j = fluid.layers.scale(j1, scale=2.0, bias=0.5)
temp7 = fluid.layers.relu(j)

cost = fluid.layers.square_error_cost(temp7, label)
avg_cost = fluid.layers.mean(cost)

optimizer = fluid.optimizer.SGD(learning_rate=0.001)
optimizer.minimize(avg_cost)

cpu = fluid.core.CPUPlace()
exe = fluid.Executor(cpu)

exe.run(fluid.default_startup_program())

fluid.io.save_inference_model("./resnet_model", [resnet_input.name], [temp7],
                              exe)
print('res', temp7.name)
