/* SPDX-License-Identifier: Apache-2.0
 * 中文：host 测试的板级头转发。直接指向仓库里的板配置，避免再复制一份会漂移的副本。
 * English: board header forwarder for host tests. It points at the repository's board config so
 * there is no second copy to drift.
 */
#pragma once
#include "../../components/kiiko_board/include/kiiko_board.h"
