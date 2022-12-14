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
"""
Load and Execute Paddle Model
=====================

In this tutorial, we will show you how to load and execute a paddle model in CINN.
We offer you four optional models: ResNet50, MobileNetV2, EfficientNet and FaceDet.
"""

import paddle
import paddle.fluid as fluid
import cinn
from cinn import *
from cinn.frontend import *
from cinn.framework import *
from cinn.common import *
import numpy as np
import os
import sys
# sphinx_gallery_thumbnail_path = './paddlepaddle.png'

##################################################################
# **Prepare to Load Model**
# --------------------------
# Declare the params and prepare to load and execute the paddle model.
#
# - :code:`model_dir` is the path where the paddle model is stored.
#
# - :code:`input_tensor` is the name of input tensor in the model.
#
# - :code:`target_tensor` is the name of output tensor we want.
#
# - :code:`x_shape` is the input tensor's shape of the model
#
# - When choosing model ResNet50, the params should be ::
#
#       model_dir = "./ResNet50"
#
#       input_tensor = 'inputs'
#
#       target_tensor = 'save_infer_model/scale_0.tmp_1'
#
#       x_shape = [1, 3, 224, 224]
#
# - When choosing model MobileNetV2, the params should be ::
#
#       model_dir = "./MobileNetV2"
#
#       input_tensor = 'image'
#
#       target_tensor = 'save_infer_model/scale_0'
#
#       x_shape = [1, 3, 224, 224]
#
# - When choosing model EfficientNet, the params should be ::
#
#       model_dir = "./EfficientNet"
#
#       input_tensor = 'image'
#
#       target_tensor = 'save_infer_model/scale_0'
#
#       x_shape = [1, 3, 224, 224]
#
# - When choosing model FaceDet, the params should be ::
#
#       model_dir = "./FaceDet"
#
#       input_tensor = 'image'
#
#       target_tensor = 'save_infer_model/scale_0'
#
#       x_shape = [1, 3, 240, 320]
#
model_dir = "./ResNet50"
input_tensor = 'inputs'
target_tensor = 'save_infer_model/scale_0.tmp_1'
x_shape = [1, 3, 224, 224]

##################################################################
# **Set the target backend**
# ------------------------------
# Now CINN only supports two backends: X86 and CUDA.
#
# - For CUDA backends, set ``target = DefaultNVGPUTarget()``
#
# - For X86 backends, set ``target = DefaultHostTarget()``
#
if os.path.exists("is_cuda"):
    target = DefaultNVGPUTarget()
else:
    target = DefaultHostTarget()

##################################################################
# **Load Model to CINN**
# -------------------------
# Load the paddle model and transform it into CINN IR.
#
# * :code:`target` is the backend to execute model on.
#
# * :code:`model_dir` is the path where the paddle model is stored.
#
# * :code:`params_combined` implies whether the params of paddle model is stored in one file.
#
#
params_combined = True
computation = Computation.compile_paddle_model(
    target, model_dir, [input_tensor], [x_shape], params_combined)

##################################################################
# **Get input tensor and set input data**
# -----------------------------------------
# Here we use random data as input. In practical applications,
# please replace it with real data according to your needs.
#
a_t = computation.get_tensor(input_tensor)
x_data = np.random.random(x_shape).astype("float32")
a_t.from_numpy(x_data, target)

##################################################################
# Here we set the output tensor's data to zero before running the model.
out = computation.get_tensor(target_tensor)
out.from_numpy(np.zeros(out.shape(), dtype='float32'), target)

##################################################################
# **Execute Model**
# -------------------------
# Execute the model and get output tensor's data.
# :code:`out` is the data of output tensor we want.
computation.execute()
res_cinn = out.numpy(target)
print("CINN Execution Done!")

##################################################################
# **Use Paddle to Verify Correctness**
# -------------------------
# Now we run the model by paddle and check if the 2 results are identical.
config = fluid.core.AnalysisConfig(model_dir + '/__model__',
                                   model_dir + '/params')
config.disable_gpu()
config.switch_ir_optim(False)
paddle_predictor = fluid.core.create_paddle_predictor(config)
data = fluid.core.PaddleTensor(x_data)
paddle_out = paddle_predictor.run([data])
res_paddle = paddle_out[0].as_ndarray()
print("Paddle Execution Done!\n =============================")
print("Verification result is: ", np.allclose(res_cinn, res_paddle, atol=1e-3))
