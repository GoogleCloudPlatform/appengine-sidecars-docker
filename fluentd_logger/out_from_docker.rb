# Copyright 2015 Google Inc. All rights reserved.
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


module Fluent
  # Logs from docker style logs to GoogleCloudOutput style logs
  class FromDockerOutput < Output
    include Configurable

    # The tag to output stdout logs
    config_param :stdout_tag, :string, :default => 'stdout'
    # The tag to output stderr logs
    config_param :stderr_tag, :string, :default => 'stderr'

    Fluent::Plugin.register_output('from_docker', self)

    def emit(tag, chunk, next_chain)
      chunk.each do |time_sec,record|
        time = Time.at(time_sec)
        message = record['log']
        new_record = {'time' => time.iso8601, 'message' => message}
        new_tag = stdout_tag
        if record['stream'] == 'stderr'
          new_tag = stderr_tag
        end
        router.emit(new_tag, time_sec, new_record)
      end
      next_chain.next
    end
  end
end
