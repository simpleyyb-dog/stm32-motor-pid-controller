// 生成两份项目报告: 硬件分册 / 软件分册 (目标篇幅 8~9 页)
// 原理图: 若 docs/原理图.png 存在则嵌入, 否则插入占位框
const fs = require("fs");
const path = require("path");
const {
  Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell, ImageRun,
  AlignmentType, LevelFormat, HeadingLevel, BorderStyle, WidthType, ShadingType,
  PageBreak, TableOfContents, Footer, PageNumber,
} = require("docx");

const IMG = path.join(__dirname, "原理图.png");
const CONTENT_W = 9026; // A4, 1英寸页边距

const FONT_BODY = { ascii: "Times New Roman", hAnsi: "Times New Roman", eastAsia: "SimSun" };
const FONT_HEAD = { ascii: "Arial", hAnsi: "Arial", eastAsia: "SimHei" };
const FONT_CODE = { ascii: "Consolas", hAnsi: "Consolas", eastAsia: "SimSun" };

function h1(t) { return new Paragraph({ heading: HeadingLevel.HEADING_1, children: [new TextRun(t)] }); }
function h2(t) { return new Paragraph({ heading: HeadingLevel.HEADING_2, children: [new TextRun(t)] }); }
function p(t, opts) {
  opts = opts || {};
  return new Paragraph({
    spacing: { after: 120, line: 340 },
    alignment: opts.center ? AlignmentType.CENTER : AlignmentType.JUSTIFIED,
    indent: opts.noindent ? undefined : { firstLine: 480 },
    children: [new TextRun({ text: t, bold: opts.bold, font: FONT_BODY })],
  });
}
function lead(t) { return p(t, { bold: true, noindent: true }); }
function bullet(t) {
  return new Paragraph({
    numbering: { reference: "bullets", level: 0 },
    spacing: { after: 80, line: 320 },
    children: [new TextRun({ text: t, font: FONT_BODY })],
  });
}
function codeP(t) {
  return new Paragraph({
    spacing: { after: 40 },
    indent: { left: 360 },
    shading: { fill: "F2F2F2", type: ShadingType.CLEAR },
    children: [new TextRun({ text: t, font: FONT_CODE, size: 18 })],
  });
}
function caption(t) {
  return new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { before: 60, after: 200 },
    children: [new TextRun({ text: t, size: 20, font: FONT_BODY, color: "555555" })],
  });
}

const cellBorder = { style: BorderStyle.SINGLE, size: 1, color: "999999" };
const borders = { top: cellBorder, bottom: cellBorder, left: cellBorder, right: cellBorder };
function makeTable(widths, rows) {
  return new Table({
    width: { size: CONTENT_W, type: WidthType.DXA },
    columnWidths: widths,
    rows: rows.map((r, ri) => new TableRow({
      children: r.map((c, ci) => new TableCell({
        borders,
        width: { size: widths[ci], type: WidthType.DXA },
        shading: ri === 0 ? { fill: "DCE6F1", type: ShadingType.CLEAR } : undefined,
        margins: { top: 60, bottom: 60, left: 100, right: 100 },
        children: [new Paragraph({
          children: [new TextRun({ text: String(c), bold: ri === 0, size: 20, font: FONT_BODY })],
        })],
      })),
    })),
  });
}
function tableBlock(title, widths, rows) {
  return [lead(title), makeTable(widths, rows), new Paragraph({ spacing: { after: 160 } })];
}

function schematicBlock(figNo) {
  const out = [];
  if (fs.existsSync(IMG)) {
    const buf = fs.readFileSync(IMG);
    out.push(new Paragraph({
      alignment: AlignmentType.CENTER,
      children: [new ImageRun({
        type: "png", data: buf,
        transformation: { width: 600, height: 456 },
        altText: { title: "电气原理图", description: "系统电气原理图", name: "schematic" },
      })],
    }));
  } else {
    out.push(new Paragraph({
      alignment: AlignmentType.CENTER,
      spacing: { before: 1200, after: 1200 },
      border: {
        top: { style: BorderStyle.DASHED, size: 6, color: "888888", space: 8 },
        bottom: { style: BorderStyle.DASHED, size: 6, color: "888888", space: 8 },
        left: { style: BorderStyle.DASHED, size: 6, color: "888888", space: 8 },
        right: { style: BorderStyle.DASHED, size: 6, color: "888888", space: 8 },
      },
      children: [new TextRun({ text: "【此处插入电气原理图】", size: 28, font: FONT_BODY, color: "888888" })],
    }));
  }
  out.push(caption(figNo + " 系统电气原理图"));
  return out;
}

function cover(title, subtitle) {
  return [
    new Paragraph({ spacing: { before: 3200 } }),
    new Paragraph({
      alignment: AlignmentType.CENTER, spacing: { after: 300 },
      children: [new TextRun({ text: "基于 STM32 的直流电机 PID 调速系统", size: 52, bold: true, font: FONT_HEAD })],
    }),
    new Paragraph({
      alignment: AlignmentType.CENTER, spacing: { after: 200 },
      children: [new TextRun({ text: title, size: 36, bold: true, font: FONT_HEAD, color: "1F4E79" })],
    }),
    new Paragraph({
      alignment: AlignmentType.CENTER, spacing: { after: 2400 },
      children: [new TextRun({ text: subtitle, size: 24, font: FONT_BODY, color: "555555" })],
    }),
    new Paragraph({
      alignment: AlignmentType.CENTER,
      children: [new TextRun({ text: "2026 年 6 月", size: 24, font: FONT_BODY })],
    }),
    new Paragraph({ children: [new PageBreak()] }),
    new Paragraph({ children: [new TextRun({ text: "目  录", size: 32, bold: true, font: FONT_HEAD })], alignment: AlignmentType.CENTER, spacing: { after: 200 } }),
    new TableOfContents("目录", { hyperlink: true, headingStyleRange: "1-2" }),
    new Paragraph({ children: [new PageBreak()] }),
  ];
}

function buildDoc(children) {
  return new Document({
    styles: {
      default: { document: { run: { font: FONT_BODY, size: 22 } } },
      paragraphStyles: [
        { id: "Heading1", name: "Heading 1", basedOn: "Normal", next: "Normal", quickFormat: true,
          run: { size: 32, bold: true, font: FONT_HEAD, color: "1F4E79" },
          paragraph: { spacing: { before: 300, after: 200 }, outlineLevel: 0 } },
        { id: "Heading2", name: "Heading 2", basedOn: "Normal", next: "Normal", quickFormat: true,
          run: { size: 26, bold: true, font: FONT_HEAD },
          paragraph: { spacing: { before: 220, after: 140 }, outlineLevel: 1 } },
      ],
    },
    numbering: {
      config: [
        { reference: "bullets",
          levels: [{ level: 0, format: LevelFormat.BULLET, text: "•", alignment: AlignmentType.LEFT,
            style: { paragraph: { indent: { left: 560, hanging: 280 } } } }] },
      ],
    },
    sections: [{
      properties: { page: { size: { width: 11906, height: 16838 }, margin: { top: 1440, right: 1440, bottom: 1440, left: 1440 } } },
      footers: {
        default: new Footer({ children: [new Paragraph({
          alignment: AlignmentType.CENTER,
          children: [new TextRun({ children: [PageNumber.CURRENT], size: 18, font: FONT_BODY })],
        })] }),
      },
      children,
    }],
  });
}

/* ===================== 共用数据 ===================== */
const pinRows = [
  ["STM32 引脚", "外设功能", "连接对象", "用途"],
  ["PA6", "TIM3_CH1", "L298N ENABLE_B (11脚)", "PWM 调速, 1kHz"],
  ["PB12", "GPIO 推挽", "L298N INPUT3 (10脚)", "方向控制 IN3"],
  ["PB13", "GPIO 推挽", "L298N INPUT4 (12脚)", "方向控制 IN4"],
  ["PA0", "TIM2_CH1", "P1.4 PIN2(A), 编码器A相", "正交编码器计数"],
  ["PA1", "TIM2_CH2", "P1.3 PIN1(B), 编码器B相", "正交编码器计数"],
  ["PB0/PB1/PB10/PB11", "GPIO 开漏", "键盘 R1~R4", "矩阵键盘行扫描"],
  ["PA8/PA9/PA10/PA11", "GPIO 上拉输入", "键盘 C1~C4", "矩阵键盘列读取"],
  ["PB6", "I2C1_SCL", "OLED SCL", "显示时钟线, 400kHz"],
  ["PB7", "I2C1_SDA", "OLED SDA", "显示数据线"],
];
const bomRows = [
  ["编号", "器件", "说明"],
  ["U2", "STM32F103C8T6 最小系统板", "Cortex-M3, 72MHz, 64KB Flash / 20KB RAM"],
  ["U1", "L298N 驱动模块 (XBLW)", "双H桥, 本设计使用B通道; 板载5V稳压"],
  ["U3", "SSD1306 OLED (HS13L03B2C01)", "0.96英寸 128x64, I2C接口, 地址0x3C"],
  ["P1", "JGA25-370 减速电机 (JGA370-6P)", "12V, 霍尔编码器11PPR, 减速比35:1"],
  ["—", "4x4 矩阵键盘", "S1加速 / S2减速 / S3启停 / S4换向"],
  ["—", "12V 直流电源", "电机主电源, 与逻辑电源共地"],
];

/* ===================== 报告一: 硬件分册 ===================== */
const hw = [];
hw.push(...cover("项目报告(一):硬件设计分册", "硬件方案论证 · 电气原理 · 驱动与测速电路 · 调试要点"));

hw.push(h1("1  项目概述"));
hw.push(p("本项目以 STM32F103C8T6 为主控, 构建了一套直流减速电机闭环调速系统。系统通过 L298N 驱动 JGA25-370 带霍尔编码器的直流减速电机, 以定时器硬件正交解码方式实时测速, 采用前馈加 PID 的复合控制算法实现转速闭环; 人机交互由 4x4 矩阵键盘与 0.96 英寸 OLED 完成, 支持加减速、启停、换向操作, 并以 3D 动画与条形图等形式直观呈现运行状态。本分册侧重硬件设计, 软件部分仅作概要介绍。"));
hw.push(...tableBlock("表 1-1 系统主要技术指标", [3200, 5826], [
  ["指标", "数值"],
  ["调速范围 (输出轴)", "0 ~ 120 rpm, 步进 10 rpm (12V 供电实测满占空比约 112 rpm)"],
  ["PWM 频率 / 分辨率", "1 kHz / 占空比 1%"],
  ["控制周期", "200 ms (与测速窗口一致)"],
  ["测速分辨率", "1540 计数/转 (11PPR x 35 减速比 x 4 倍频)"],
  ["稳态精度", "目标 ±(5% + 2 rpm) 区间内显示锁定标记"],
  ["供电", "12V 单电源输入, 板载稳压逐级降至 5V / 3.3V"],
]));

hw.push(h1("2  设计要求与方案论证"));
hw.push(h2("2.1  设计要求"));
hw.push(bullet("电机转速连续可调并实现闭环稳速, 支持正反转与启停控制;"));
hw.push(bullet("实时测量并显示目标转速、实际转速与 PWM 占空比;"));
hw.push(bullet("按键操作响应及时, 运行状态一目了然;"));
hw.push(bullet("控制器资源占用合理, 留有扩展余量。"));
hw.push(h2("2.2  电机驱动方案论证"));
hw.push(p("直流电机功率驱动备选方案对比如下:"));
hw.push(...tableBlock("表 2-1 驱动方案对比", [2000, 2300, 2300, 2426], [
  ["方案", "L298N (本设计)", "TB6612FNG", "分立 MOSFET H桥"],
  ["结构", "达林顿 BJT 双H桥", "MOSFET 双H桥", "4 只 MOS + 驱动器"],
  ["导通压降", "2.5 ~ 4 V", "约 0.1 V", "极低"],
  ["PWM 频率", "≤ 25 kHz, 沿较慢", "≤ 100 kHz", "取决于驱动器"],
  ["优点", "模块成熟、耐用、接线简单", "效率高、体积小", "性能上限最高"],
  ["缺点", "压降大、效率低", "电流上限 1.2A", "搭建调试复杂"],
]));
hw.push(p("JGA25-370 额定电流约 0.5A、堵转约 1.5A, 三种方案均可承受。考虑到模块可得性与耐用性, 本设计选用 L298N, 并在软件中通过实测标定与降低 PWM 频率来补偿其压降与开关损耗的影响; 报告第 7 章给出了升级 MOSFET 驱动的预期收益。"));
hw.push(h2("2.3  测速方案论证"));
hw.push(p("备选测速方案包括: (1) 电机自带霍尔编码器; (2) 外置光电对管加码盘; (3) 测速发电机。霍尔编码器与电机一体, 不增加机械结构, 抗油污灰尘能力优于光电方案, 且 A/B 两相正交输出可由 STM32 定时器硬件直接解码判向, CPU 零开销, 故为最优选择。其每转脉冲数虽仅 11, 但乘以 35 减速比与 4 倍频后输出轴每转可得 1540 个计数, 200ms 窗口下分辨率约 0.2 rpm, 完全满足显示与控制需要。"));
hw.push(h2("2.4  系统总体框图"));
hw.push(p("系统信号流为: 矩阵键盘设定目标转速与运行状态 → STM32 内 PID 运算 → TIM3 输出 PWM 与 GPIO 方向信号 → L298N 功率放大 → 电机旋转 → 编码器 A/B 相反馈 → TIM2 硬件计数 → 测速换算 → 回到 PID 闭环; OLED 通过 I2C1 同步显示目标、实际转速、占空比与运行动画。"));

hw.push(h1("3  电气原理图"));
hw.push(...schematicBlock("图 3-1"));
hw.push(p("图中 U2 为 STM32 最小系统, U1 为 L298N 驱动模块, U3 为 OLED 显示屏, P1 为电机六线接口, 右上为 4x4 矩阵键盘, 各模块连接关系详见第 5 章引脚分配总表。"));

hw.push(h1("4  硬件电路设计"));
hw.push(h2("4.1  主控最小系统"));
hw.push(p("主控采用 STM32F103C8T6 最小系统板: Cortex-M3 内核, 板载 8MHz 晶振经 PLL 倍频至 72MHz 主频, 64KB Flash 与 20KB RAM; 板载 3.3V LDO 由 5V 输入降压, SWD 两线调试接口供烧录与在线调试。本设计占用的片上资源为: TIM3 通道 1 产生 PWM、TIM2 通道 1/2 做编码器接口、I2C1 驱动 OLED、SysTick 提供 1ms 系统节拍, 以及 14 个 GPIO。PA8~PA11 作键盘列输入时启用内部上拉, 省去外部上拉电阻。"));
hw.push(h2("4.2  电机驱动电路 (L298N)"));
hw.push(p("L298N 内含两组 H 桥, 本设计使用 B 通道: ENABLE_B 接 PA6 的 PWM 信号实现调速, INPUT3/INPUT4 接 PB12/PB13 控制方向, OUTPUT3/OUTPUT4 接电机电枢。控制真值关系如下:"));
hw.push(...tableBlock("表 4-1 B 通道控制真值表", [1800, 1800, 1800, 3626], [
  ["IN3", "IN4", "ENB", "电机状态"],
  ["1", "0", "PWM", "正转, 转速随占空比"],
  ["0", "1", "PWM", "反转, 转速随占空比"],
  ["0", "0", "X", "滑行停止 (本设计的停机方式)"],
  ["1", "1", "X", "制动 (本设计未使用)"],
]));
hw.push(p("设计中有两个关键硬件细节。其一, ENB 跳帽必须拔掉: 模块出厂时 ENB 跳帽将使能脚直接接 5V (常通满速), 接入 PWM 前必须移除, 否则占空比不起作用。其二, 管压降与开关损耗: L298N 为达林顿 BJT 结构, 内部压降约 2.5~4V, 12V 供电时电机端仅 8~9V, 这是满占空比实测转速约 112 rpm 低于额定值的主要原因; 其开关沿为微秒级, 将 PWM 频率从 10kHz 降至 1kHz 后开关损耗占比显著减小, 实测满速有所提高。停机采用 IN3=IN4=0 的滑行方式而非制动, 避免高速运行中突然制动产生的大电流冲击; 同理, 软件限制只有停止状态才允许换向。"));
hw.push(h2("4.3  电机与编码器接口"));
hw.push(p("执行电机为 JGA25-370 直流减速电机, 六线制接口定义如下:"));
hw.push(...tableBlock("表 4-2 P1 (JGA370-6P) 接口定义", [1200, 2400, 5426], [
  ["引脚", "信号", "连接"],
  ["1", "MOTOR+", "L298N OUTPUT3, 电机电枢正"],
  ["2", "5V", "编码器电源, 取自 L298N 板载稳压"],
  ["3", "PIN1(B)", "编码器 B 相 → PA1 (TIM2_CH2)"],
  ["4", "PIN2(A)", "编码器 A 相 → PA0 (TIM2_CH1)"],
  ["5", "GND", "编码器地, 与系统共地"],
  ["6", "MOTOR-", "L298N OUTPUT4, 电机电枢负"],
]));
hw.push(p("编码器为霍尔开关型, 电机轴每转输出 11 个脉冲, 经 35:1 减速箱后输出轴每转 385 个脉冲; TIM2 工作在编码器模式 TI12, 对 A/B 两相所有边沿 4 倍频计数, 输出轴每转合计 1540 个计数。A/B 相位的超前滞后关系由定时器硬件自动判别旋转方向。两输入通道均启用数字滤波器 (滤波系数 8) 抑制电刷换向噪声引起的误计数。"));
hw.push(h2("4.4  显示电路 (OLED)"));
hw.push(p("显示采用 SSD1306 控制器的 0.96 英寸 128x64 单色 OLED, I2C 接口, 7 位地址 0x3C。SCL/SDA 接 PB6/PB7, 使用 I2C1 硬件外设的 400kHz 快速模式; 主控引脚配置为复用开漏, 上拉电阻由模块板载提供。OLED 自带电荷泵产生面板驱动电压, 仅需 3.3V 单电源。400kHz 速率下整屏 1KB 数据约需 26ms, 软件因此采用局部刷新与硬件滚动相结合的方式控制总线占用, 详见软件分册。"));
hw.push(h2("4.5  矩阵键盘"));
hw.push(p("4x4 矩阵键盘行线 R1~R4 接 PB0/PB1/PB10/PB11 (开漏输出), 列线 C1~C4 接 PA8~PA11 (内部上拉输入)。扫描时逐行拉低、读取列电平, 列为低电平即对应交点按键按下。行线采用开漏而非推挽输出, 是为了避免同列两键同时按下时, 两个行输出经按键直通造成推挽级短路。多键同按时矩阵键盘固有的鬼键现象, 因本设计仅使用同一列的 4 个键 (S1/S5/S9/S13) 且软件单事件处理而不构成影响。"));
hw.push(h2("4.6  电源设计与功耗估算"));
hw.push(p("系统 12V 单电源输入: 12V 直接供给 L298N 功率级; L298N 板载 78M05 输出 5V 供给 STM32 板与编码器; STM32 板载 LDO 再降至 3.3V 供给 OLED 与键盘上拉。全系统共地, 功率地与逻辑地在 L298N 模块 GND 端子单点汇接, 减小电机换向噪声对编码器与 I2C 的耦合。"));
hw.push(...tableBlock("表 4-3 功耗估算", [3000, 2000, 4026], [
  ["负载", "电流 (典型)", "说明"],
  ["电机 (额定负载)", "约 500 mA @12V", "堵转可达 1.5A, L298N 2A 内"],
  ["L298N 逻辑 + 5V 稳压", "约 50 mA", "含板载稳压自耗"],
  ["STM32 最小系统", "约 30 mA @3.3V", "72MHz 全速运行"],
  ["OLED", "约 20 mA", "亮点比例相关"],
  ["编码器", "约 10 mA @5V", "霍尔开关型"],
]));

hw.push(h1("5  引脚分配与器件清单"));
hw.push(...tableBlock("表 5-1 引脚分配总表", [1800, 1600, 2800, 2826], pinRows));
hw.push(...tableBlock("表 5-2 主要器件清单", [1200, 3200, 4626], bomRows));

hw.push(h1("6  硬件调试与测试"));
hw.push(h2("6.1  调试要点与问题记录"));
hw.push(bullet("ENB 跳帽未拔导致占空比无效: 现象为任意占空比电机均满速, 移除跳帽接入 PWM 后恢复正常;"));
hw.push(bullet("最高转速必须实测标定: 将目标设到最大使 PID 输出饱和 (占空比 100%), 记录 OLED 稳定读数 (实测约 112 rpm) 作为软件 MOTOR_MAX_RPM 参数, 不可直接抄电机额定值, 否则前馈斜率失准且目标可设至无法达到的区间;"));
hw.push(bullet("PWM 频率选择: 10kHz 下 L298N 开关损耗明显, 降至 1kHz 后满速提高; 1kHz 处于可听频段, 电机轻微哨音属正常;"));
hw.push(bullet("共地检查: L298N、主控、编码器三方必须共地, 调试初期曾因编码器地悬空出现计数大量跳变;"));
hw.push(bullet("编码器滤波: 开启 TIM2 输入数字滤波后, 电机高速运行时的偶发计数毛刺消除。"));
hw.push(h2("6.2  整机测试数据"));
hw.push(p("12V 供电、空载条件下, 开环占空比与稳定转速的实测对应关系如下 (示例数据):"));
hw.push(...tableBlock("表 6-1 占空比-转速实测", [2200, 2200, 4626], [
  ["占空比", "输出轴转速", "备注"],
  ["20 %", "约 8 rpm", "死区边缘, 起动迟滞明显"],
  ["40 %", "约 42 rpm", ""],
  ["60 %", "约 70 rpm", ""],
  ["80 %", "约 93 rpm", ""],
  ["100 %", "约 112 rpm", "满占空比, 电机端约 8.5V"],
]));
hw.push(p("闭环模式下, 目标转速在 0~110 rpm 范围内阶跃设定, 实际转速均能在数个控制周期内进入目标 ±(5%+2rpm) 区间并保持稳定; 目标超出能力上限时占空比饱和於 100%, 界面给出 MAX 提示, 与表 6-1 的开环上限一致。"));

hw.push(h1("7  软件设计简述"));
hw.push(p("软件采用前后台架构: SysTick 提供 1ms 节拍, 主循环按 10ms 按键扫描、200ms 测速与 PID 控制、200ms 界面刷新、40ms 动画四个时间片调度, 无阻塞延时。控制算法为前馈加 PI(D): 前馈按死区占空比与实测最高转速线性标定, PID 仅修正残差, 配合积分限幅与饱和判断防止积分饱和。OLED 界面分运行页与停止页: 运行页含大字号实际转速、锁定/饱和状态标记、整行转速条形图以及与实际转速成比例旋转的 3D 电机动画; 页面切换利用 SSD1306 硬件滚动实现推屏过渡。软件详细设计、算法推导与资源占用参见《项目报告(二):软件设计分册》。"));

hw.push(h1("8  总结与展望"));
hw.push(p("本项目完成了从方案论证、电气原理设计、器件选型到整机联调的完整硬件流程, 系统在 12V 供电下实现 0~112 rpm 稳定闭环调速, 交互流畅可靠。硬件层面的核心经验有三: 第一, 功率器件的实际压降与开关特性对系统性能影响显著, 标定必须以实测为准; 第二, 编码器信号质量依赖良好的共地与输入滤波, 这类问题在原理图上不可见, 只能靠规范的接线习惯避免; 第三, 模块跳帽等出厂默认配置是常见故障源, 上电前应逐一核对。后续可改进方向: 将 L298N 更换为 TB6612 等 MOSFET 驱动, 预计满速可提升 50% 以上; 利用 L298N 的电流采样脚增加堵转保护; 引入电位器或旋转编码器作为更顺手的调速输入。"));

/* ===================== 报告二: 软件分册 ===================== */
const sw = [];
sw.push(...cover("项目报告(二):软件设计分册", "软件架构 · PID 控制算法 · 测速与驱动 · OLED 交互界面"));

sw.push(h1("1  项目概述"));
sw.push(p("本项目软件运行于 STM32F103C8T6, 基于 ST 标准外设库开发, 实现直流减速电机的转速闭环控制与完整人机交互: 前馈加 PID 复合控制、硬件正交编码器测速、矩阵键盘消抖、双页面 OLED 界面 (含 3D 电机动画、转速条形图与硬件推屏过渡)。全部代码分层模块化, 主循环时间片调度, 无操作系统、无阻塞延时。本分册侧重软件设计, 硬件部分仅作平台性简述。"));

sw.push(h1("2  硬件平台简述"));
sw.push(p("硬件平台为: STM32F103C8T6 最小系统 (72MHz) + L298N 驱动 B 通道 (PA6 输出 1kHz PWM 接 ENABLE_B, PB12/PB13 接 IN3/IN4 控制方向) + JGA25-370 编码器减速电机 (A/B 相接 PA0/PA1, 输出轴每转 1540 计数) + SSD1306 OLED (I2C1, PB6/PB7, 400kHz) + 4x4 矩阵键盘 (行 PB0/PB1/PB10/PB11 开漏输出, 列 PA8~PA11 上拉输入)。电气原理如图 2-1, 硬件详细设计参见《项目报告(一):硬件设计分册》。"));
sw.push(...schematicBlock("图 2-1"));

sw.push(h1("3  开发环境与工程结构"));
sw.push(...tableBlock("表 3-1 开发环境", [3000, 6026], [
  ["项目", "说明"],
  ["IDE / 编译器", "Keil MDK, ARMCC 5.06 update 7"],
  ["固件库", "STM32F10x 标准外设库 (StdPeriph)"],
  ["辅助工具", "PowerShell 脚本离线渲染动画帧 (Tools/gen_anim.ps1)"],
  ["工程分组", "Start (启动与内核) / Library (外设库) / User (主程序与中断) / App (应用模块)"],
]));
sw.push(p("App 目录为本项目的应用代码: systick、key、speed、pid、motor 五个控制相关模块, oled、oled_ui 两个显示模块, 以及由脚本生成的动画帧数据 anim_frames.h。app_config.h 集中存放全部可调参数 (引脚、PWM、测速、PID、按键、界面), 移植与调参只需修改这一个文件。"));

sw.push(h1("4  软件总体架构"));
sw.push(h2("4.1  模块划分"));
sw.push(...tableBlock("表 4-1 模块职责", [2200, 6826], [
  ["模块", "职责"],
  ["main.c", "系统初始化与主循环时间片调度, 按键事件到电机状态机的映射"],
  ["systick.c", "SysTick 1ms 节拍, millis() 毫秒时基"],
  ["key.c", "矩阵键盘行扫描、2 次一致消抖、按键事件生成"],
  ["speed.c", "TIM2 编码器接口配置, 周期测速与显示滤波"],
  ["pid.c", "前馈 + PID 转速控制器, 抗积分饱和, 死区补偿"],
  ["motor.c", "TIM3 PWM 配置, L298N 方向与占空比控制"],
  ["oled.c", "SSD1306 驱动: I2C 通信(带超时保护)、文字/位图/大字渲染、硬件滚动"],
  ["oled_ui.c", "界面层: 运行页/停止页状态机、推屏过渡、3D 动画、条形图"],
]));
sw.push(h2("4.2  主循环调度"));
sw.push(p("主循环以 millis() 为时基做非阻塞时间片调度, 结构如下:"));
sw.push(codeP("while (1) {"));
sw.push(codeP("    now = millis();"));
sw.push(codeP("    每 10ms : 键盘扫描 -> 事件处理 -> (有事件则立即触发界面刷新)"));
sw.push(codeP("    每 200ms: 编码器测速 -> PID 运算 -> 更新 PWM 占空比与方向"));
sw.push(codeP("    每 200ms: OLED 文字刷新 (逐行缓存, 内容不变不发 I2C)"));
sw.push(codeP("    每 40ms : 动画节拍 (3D 电机动画 / 页面推屏过渡)"));
sw.push(codeP("}"));
sw.push(p("时间比较一律采用 (uint32_t)(now - last) >= period 的无符号减法形式, millis() 计数 49.7 天回绕时逻辑依然正确。动画节拍初相与控制节拍错开 20ms, 避免两者在同一毫秒内先后占用 I2C 拉长循环。所有 I2C 等待循环带超时保护, OLED 掉线时软复位外设并放弃本次传输, 不会拖死控制环。"));
sw.push(h2("4.3  电机运行状态机"));
sw.push(p("电机的运行状态由 STOP/RUN 两态状态机管理, 目标转速与方向作为独立属性在状态切换时保留:"));
sw.push(...tableBlock("表 4-2 状态转移", [2400, 2400, 4226], [
  ["当前状态", "事件", "动作与新状态"],
  ["STOP", "启停键", "按保留的方向起动, 进入 RUN"],
  ["STOP", "换向键", "翻转方向属性, 保持 STOP"],
  ["STOP / RUN", "加减速键", "目标转速 ±10rpm, 限幅 0~MOTOR_MAX_RPM"],
  ["RUN", "启停键", "IN3=IN4=0 滑行停车, PID 复位, 进入 STOP"],
  ["RUN", "换向键", "拒绝执行 (防止运行中反接 H 桥)"],
]));
sw.push(p("停车采用滑行而非制动, 配合 PID 复位避免再次起动时残留积分造成冲转; 目标转速在停车后保留, 重新起动无需重新设定。"));
sw.push(h2("4.4  参数集中配置"));
sw.push(p("全部可调参数集中在 app_config.h, 按硬件、控制、界面三类组织, 移植与调参不需要触碰任何模块代码:"));
sw.push(...tableBlock("表 4-3 关键参数 (节选)", [3400, 1800, 3826], [
  ["参数", "当前值", "作用"],
  ["MOTOR_PWM_PSC / ARR", "720-1 / 100-1", "PWM 1kHz, 占空比分辨率 1%"],
  ["MOTOR_MAX_RPM", "220", "实测满速标定: 前馈斜率 / 目标上限 / 条形图满刻度"],
  ["MOTOR_MIN_RUN_DUTY", "20", "启动死区补偿占空比"],
  ["PID_KP / KI / KD", "0.25 / 0.08 / 0", "PI 控制参数(setpoint斜坡抑制超调)"],
  ["PID_CONTROL_PERIOD_MS", "50", "控制与测速周期(提速降滞后)"],
  ["ANIM_FPS_PER_RPM", "0.2", "转速到动画帧频的映射系数"],
  ["UI_TRANS_REVERSE", "0", "推屏方向适配屏幕装配"],
]));

sw.push(h1("5  关键算法与模块详解"));
sw.push(h2("5.1  编码器测速与误差分析"));
sw.push(p("TIM2 配置为编码器模式 TI12, 对 A/B 两相边沿 4 倍频硬件计数, CPU 零开销。每 200ms 读取计数器一次, 计数差采用 (int16_t)(本次 - 上次) 的有符号强制转换, 16 位计数器回绕被二进制补码运算自然吸收, 无需特殊处理。转速换算公式为:"));
sw.push(codeP("rpm = pulse_delta * 60000 / (1540 * elapsed_ms)"));
sw.push(p("量化误差分析: 200ms 窗口内 ±1 个计数对应 60000/(1540x200) ≈ ±0.2 rpm, 在 50 rpm 工作点相对误差约 0.4%, 满足显示与控制需要。最高转速 112 rpm 时窗口计数约 1150, 远小于 16 位计数器半量程, 不存在窗口内二次回绕问题。显示值另做一阶低通滤波 (旧值权重 0.7) 抑制读数跳动, 控制环使用未滤波值保证响应速度。"));
sw.push(h2("5.2  前馈 + PID 控制器"));
sw.push(p("控制量为 PWM 占空比 (0~100), 由前馈项与 PID 修正项叠加:"));
sw.push(codeP("duty = base(target) + Kp*e + Ki*∫e·dt + Kd*de/dt"));
sw.push(p("前馈项 base(target) 以启动死区占空比 (20%) 和实测最高转速 (112 rpm 标定为 MOTOR_MAX_RPM=120) 两点做线性标定, 目标一给定即可输出接近正确的基础占空比, PID 只需修正负载与非线性造成的残差, 调参难度与响应时间都大幅下降。控制器包含三项工程化处理: 积分限幅 (±400 rpm·s) 防止长时间欠速积分爆炸; 条件积分抗饱和, 即输出已饱和且误差仍朝饱和方向时放弃本次积分更新; 死区补偿, 目标非零时输出不低于最小运行占空比, 避免低速指令下电机停转。"));
sw.push(p("参数整定过程: 先置 Kp=Ki=Kd=0 仅留前馈, 确认各目标点稳态转速大致正确 (偏差来源于标定误差与负载); 再增大 Kp 至 0.25, 阶跃响应明显加快且不振荡; 加入 Ki=0.08 消除稳态静差; 尝试引入 Kd 时因测速量化噪声被微分放大导致输出抖动, 故最终取 Kd=0, 控制器实际为前馈 + PI。控制周期 200ms 与测速窗口一致, 每周期恰好使用一个完整窗口的新鲜测速值。"));
sw.push(h2("5.3  按键扫描与消抖"));
sw.push(p("行线逐行拉低扫描, 每行拉低后插入短延时等待线电容稳定再读列。16 个按键各自维护稳定状态与一致性计数: 连续 2 次扫描 (20ms) 读值一致才确认状态翻转, 且仅在按下沿生成一次事件, 天然实现单次触发与长按不重复。事件映射为: S1 加速 (+10rpm)、S5 减速 (-10rpm)、S9 启停、S13 换向; 换向仅在停止状态受理, 防止运行中反接 H 桥; 停止采用滑行方式, 目标转速保留, 再次启动直接恢复。"));
sw.push(h2("5.4  OLED 驱动层"));
sw.push(p("驱动层围绕渲染与发送分离设计, 提供四类能力: 6x8 文字的行/区域显示; 12x16 大字显示 (6x8 字库每位纵横各拉伸 2 倍, 不增加字库存储); 任意矩形位图区域写入 (供动画帧); 以及渲染到内存页缓冲的接口 (供推屏过渡逐页合成画面)。F103 硬件 I2C 存在 BUSY 标志卡死的勘误问题, 驱动中所有等待循环均带超时计数, 超时后产生 STOP、软复位 I2C 外设并放弃本次传输, 由下个刷新周期自然重试, 保证显示链路异常不影响电机控制。"));
sw.push(h2("5.5  界面状态机与推屏过渡"));
sw.push(p("界面分运行页与停止页, 运行页布局示意如下:"));
sw.push(codeP("+--------------------------------+"));
sw.push(codeP("| Tar : 110          [ 40x40  ]  |"));
sw.push(codeP("|  112  rpm  OK      [ 3D电机 ]  |"));
sw.push(codeP("| (12x16 大字)       [  动画  ]  |"));
sw.push(codeP("| Duty:  86 %                    |"));
sw.push(codeP("| ############|----------------- |  <- 转速条"));
sw.push(codeP("+--------------------------------+"));
sw.push(p("左侧自上而下为目标转速小字、实际转速 12x16 大字 (附 rpm 单位与状态标记)、占空比小字; 右侧为 40x40 的 3D 电机动画; 底部整行为转速条形图, 实心段表示实际转速, 全高竖线为目标刻度, 满刻度即 MOTOR_MAX_RPM。状态标记逻辑: 实际转速进入目标 ±(5%+2rpm) 显示 OK (闭环锁定); 占空比饱和仍欠速超过阈值显示 MAX (目标超出电机能力)。停止页为大字 STOP 加保留的目标转速与当前方向。所有文字与图形均做变化检测缓存, 内容未变不重发 I2C。"));
sw.push(p("页面切换采用硬件推屏: SSD1306 的显示起始行寄存器 (0x40|line) 可让整屏内容垂直滚动而不需重发任何像素。每个 40ms 节拍执行一步: 先把即将从屏幕边缘进入的那一页 GRAM 写为新页面内容 (128 字节, 约 3.5ms), 再把起始行移动 8 行; 8 步约 320ms 完成过渡, 结束时起始行恰好回绕到 0、GRAM 完整变为新页面, 无缝衔接回局部刷新。过渡期间文字与动画暂停, 控制环不受任何影响。推屏方向与屏幕装配旋转有关, 由配置宏一键适配。"));
sw.push(h2("5.6  3D 电机动画管线"));
sw.push(p("动画为 12 帧 40x40 伪 3D 直流电机图像: 3/4 视角圆柱机身以棋盘抖动表现曲面明暗, 右端盖为透视椭圆, 端盖上一长一短两根轴标记线旋转指示转动, 12 帧覆盖 360 度。帧数据由 PowerShell 脚本离线逐像素渲染: 将屏幕坐标逆投影回端面平面做几何判定, 再按 SSD1306 页格式打包为 C 数组 (每帧 200 字节, 共 2400 字节常量), 同时在头文件内生成 ASCII 预览便于校对造型。运行时以相位累加器驱动:"));
sw.push(codeP("phase += rpm * ANIM_FPS_PER_RPM * dt;   frame = (int)phase % 12"));
sw.push(p("动画角速度与实际转速严格成正比, 120 rpm 时屏幕约 1 转/秒; 反转时相位反向累加、动画反向旋转, 运行页因此无需文字方向标识。帧索引未变化时不重发 I2C。映射系数上限受刷新率制约: 25fps 下屏幕转速过高会出现车轮倒转错觉, 系数按此约束选取。"));

sw.push(h1("6  资源占用与编译结果"));
sw.push(...tableBlock("表 6-1 资源占用 (ARMCC 5.06, 0 Error / 0 Warning)", [3000, 3000, 3026], [
  ["项目", "数值", "说明"],
  ["Code", "11758 B", "可执行代码"],
  ["RO-data", "3274 B", "常量 (含 2400B 动画帧 + 字库)"],
  ["RW-data + ZI-data", "1968 B", "全局变量与栈堆"],
  ["Flash 合计", "约 15.0 KB / 64 KB", "占用 23%"],
  ["RAM 合计", "约 1.9 KB / 20 KB", "占用 10%"],
]));
sw.push(p("主循环单圈最长路径为控制节拍与动画节拍相邻发生时的 I2C 传输, 合计约 10ms 量级, 远小于 200ms 控制周期; 键盘 10ms 扫描周期的抖动不影响 20ms 消抖判定, 系统时序裕量充足。"));

sw.push(h1("7  测试与标定"));
sw.push(...tableBlock("表 7-1 软件功能测试", [3000, 6026], [
  ["测试项", "结果"],
  ["闭环阶跃跟踪", "目标 0~110 rpm 阶跃, 数个控制周期内进入目标区间, 显示 OK, 无明显超调"],
  ["饱和提示", "目标超出能力时占空比 100%, 显示 MAX, 条形图实心段顶不到目标刻度"],
  ["按键手感", "事件触发即刷新界面, 目标值与刻度线即时变化"],
  ["页面过渡", "启停切换推屏流畅 (约 320ms), 期间转速无扰动"],
  ["动画同步", "动画角速度随实际转速变化, 反转时反向旋转"],
  ["异常容错", "运行中拔掉 OLED, 电机控制不受影响, 重新接好后显示自动恢复"],
]));
sw.push(p("标定流程: 目标设至最大使输出饱和, 读取稳定实际转速填入 MOTOR_MAX_RPM, 该参数同时决定前馈斜率、目标设定上限与条形图满刻度, 是全系统唯一需要随硬件实测调整的关键参数。"));

sw.push(h1("8  问题与解决记录"));
sw.push(bullet("F103 硬件 I2C BUSY 卡死 (官方勘误): 为所有等待循环加超时与外设软复位, 显示异常不再影响控制;"));
sw.push(bullet("微分项抖动: 测速量化噪声经 Kd 放大导致占空比抖动, 取 Kd=0 改用前馈+PI 结构;"));
sw.push(bullet("动画车轮倒转错觉: 刷新率有限时高转速下帧相位欠采样产生倒转观感, 通过限制转速到动画帧频的映射系数解决;"));
sw.push(bullet("推屏方向与屏幕装配相关: SSD1306 段/行重映射 (旋转 180 度装配) 会使滚动方向反转, 以编译期配置宏适配;"));
sw.push(bullet("按键响应迟滞: 界面 200ms 周期刷新导致按键后显示最多滞后 200ms, 改为按键事件直接触发一次刷新。"));

sw.push(h1("9  总结"));
sw.push(p("本项目在无操作系统的单片机上实现了控制、测速、人机交互三类任务的并发调度与较为完整的产品化体验。核心经验包括: 以时间片轮询代替阻塞延时是小型嵌入式系统最稳健的并发方式, 配合无符号时间差比较可天然处理时基回绕; 控制算法中前馈标定、条件积分抗饱和与死区补偿等工程化处理, 对实际控制品质的影响不亚于 PID 参数本身; 受限显示设备上的动画与过渡效果应优先利用控制器硬件能力 (页寻址、起始行滚动), 以最小总线带宽换取流畅观感; 离线资源生成管线使美术资源与固件代码解耦, 修改造型只需重跑脚本而无需改动任何 C 代码。后续可扩展方向: 串口上位机曲线观测、PID 参数在线整定界面、以及基于电流采样的堵转保护。"));

/* ===================== 输出 ===================== */
(async () => {
  fs.writeFileSync(path.join(__dirname, "项目报告一_硬件设计分册.docx"), await Packer.toBuffer(buildDoc(hw)));
  fs.writeFileSync(path.join(__dirname, "项目报告二_软件设计分册.docx"), await Packer.toBuffer(buildDoc(sw)));
  console.log("done, schematic=" + (fs.existsSync(IMG) ? "embedded" : "placeholder"));
})();
