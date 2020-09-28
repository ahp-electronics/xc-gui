using System;
using System.Windows.Forms;
using System.Drawing;
using System.Drawing.Imaging;
using System.Windows.Forms.DataVisualization;
using System.Collections.Generic;
using System.Linq;
using System.IO.Ports;
using System.Threading.Tasks;
using System.Threading;

namespace Crosscorrelator
{
	public class MainForm : Form
	{
		Panel panel = new Panel ();
		Button start = new Button ();
		Button stop = new Button ();
		Graph[] chart;
		XC correlator;
		double TimeScale = 10.0;

		public MainForm ()
		{
			correlator = new XC ();
			correlator.Connected += Correlator_Connected;
			this.ClientSize = new Size (720, 480);
			this.Shown += MainForm_Shown;
		}

		void Correlator_Connected (object sender, ConnectionEventArgs e)
		{
			if (e.Connected) {
				correlator.SweepUpdate += Correlator_SweepUpdate;
				Pen[] pens1 = new Pen[correlator.NumBaselines];
				for (int x = 0; x < correlator.NumLines; x++) {
					int ix = x * 256 / correlator.NumLines;
					pens1 [x] = new Pen (Color.FromArgb ((ix >= 0 && ix < 128) ? ix + 128 : 0, (ix >= 64 && ix < 192) ? ix + 64 : 0, (ix >= 128 && ix < 256) ? ix : 0));
				}
				Pen[] pens2 = new Pen[correlator.NumBaselines];
				for (int x = 0; x < correlator.NumBaselines; x++) {
					int ix = x * 256 / correlator.NumBaselines;
					pens2 [x] = new Pen (Color.FromArgb ((ix >= 0 && ix < 128) ? ix + 128 : 0, (ix >= 64 && ix < 192) ? ix + 64 : 0, (ix >= 128 && ix < 256) ? ix : 0));
				}
				start = new Button ();
				start.Size = new Size (80, 23);
				start.Location = new Point (640, 3);
				start.Text = "Run";
				start.TextAlign = ContentAlignment.MiddleCenter;
				start.Click += button_Click;
				Controls.Add (start);
				ComboBox mode;
				stop = new Button ();
				stop.Size = new Size (80, 23);
				stop.Location = new Point (640, 29);
				stop.Text = "Stop";
				stop.TextAlign = ContentAlignment.MiddleCenter;
				stop.Click += button_Click;
				Controls.Add (stop);
				mode = new ComboBox ();
				mode.DropDownStyle = ComboBoxStyle.DropDownList;
				mode.Size = new Size (75, 23);
				mode.Location = new Point (640, 55);
				mode.Items.AddRange (new object[] { "Counter", "Autocorrelator", "Crosscorrelator" });
				mode.SelectedIndexChanged += (object s, EventArgs ev) => {
					correlator.OperatingMode = (OperatingMode)(((ComboBox)s).SelectedIndex);
				};
				mode.SelectedIndex = 0;
				Controls.Add (mode);
				Label label;
				label = new Label ();
				label.Size = new Size (75, 23);
				label.Location = new Point (485, 5);
				label.Text = "Sampling div.";
				Controls.Add (label);
				NumericUpDown freqdiv;
				freqdiv = new NumericUpDown ();
				freqdiv.Size = new Size (75, 23);
				freqdiv.Location = new Point (560, 3);
				freqdiv.Minimum = 0;
				freqdiv.Maximum = 63;
				freqdiv.Value = 0;
				freqdiv.ValueChanged += Freqdiv_ValueChanged;
				Controls.Add (freqdiv);
				label = new Label ();
				label.Size = new Size (75, 23);
				label.Location = new Point (325, 3);
				label.Text = "X scale";
				Controls.Add (label);
				NumericUpDown scalex;
				scalex = new NumericUpDown ();
				scalex.Size = new Size (75, 23);
				scalex.Location = new Point (400, 3);
				scalex.Minimum = 1;
				scalex.Maximum = 100;
				scalex.Value = 1;
				scalex.ValueChanged += Scalex_ValueChanged;
				Controls.Add (scalex);
				label = new Label ();
				label.Size = new Size (75, 23);
				label.Location = new Point (165, 3);
				label.Text = "Baudrate";
				Controls.Add (label);
				ComboBox baudrate;
				baudrate = new ComboBox ();
				baudrate.DropDownStyle = ComboBoxStyle.DropDownList;
				baudrate.Size = new Size (75, 23);
				baudrate.Location = new Point (240, 3);
				baudrate.Items.AddRange (new object[] { "57600", "115200", "230400" });
				baudrate.SelectedIndexChanged += Baudrate_SelectedIndexChanged;
				baudrate.SelectedIndex = 0;
				Controls.Add (baudrate);
				panel = new Panel ();
				panel.AutoScroll = true;
				panel.Size = new Size (720, 480 - (33 + 30 * (correlator.NumLines / 4)));
				panel.Location = new Point (0, 33 + 30 * (correlator.NumLines / 4));
				Controls.Add (panel);
				chart = new Graph[correlator.NumLines * 3 + correlator.NumBaselines];

				counts = new double[correlator.NumLines][];
				lines = new double[correlator.NumLines][];
				for (int x = 0; x < correlator.NumLines; x++) {
					label = new Label ();
					label.Text = "Line " + (x + 1);
					label.Size = new Size (50, 18);
					label.Location = new Point (20 + 160 * (x % 4), 35 + (30 * (x / 4)));
					Controls.Add (label);
					ComboBox line;
					line = new ComboBox ();
					line.DropDownStyle = ComboBoxStyle.DropDownList;
					line.Size = new Size (75, 23);
					line.Location = new Point (80 + 160 * (x % 4), 33 + (30 * (x / 4)));
					line.Name = "line" + x;
					line.Text = "Line " + (x + 1);
					line.Items.AddRange (new object[] { "Off", "On", "On,Power", "Power" });
					line.SelectedIndexChanged += Line_SelectedIndexChanged;
					line.SelectedIndex = 1;
					Controls.Add (line);
					counts [x] = new double[60];
					lines [x] = new double[correlator.DelaySize];
				}
				int idx = 0;
				for (int x = 0; x < correlator.NumLines; x++) {
					chart [idx] = new Graph ();
					chart [idx].DotsPen = pens1 [x];
					chart [idx].Size = new Size (690, 200);
					chart [idx].Location = new Point (0, 0);
					if (x == 0) {
						chart [idx].LabelX = "Time (s)";
						chart [idx].LabelY = "Counts";
						panel.Controls.Add (chart [idx]);
					} else {
						chart [idx].BackColor = Color.Transparent;
						chart [idx].ShowScale = false;
						chart [idx - 1].Controls.Add (chart [idx]);
					}
					chart [idx].StartX = 1;
					chart [idx].StartY = 0;
					chart [idx].EndX = TimeScale;
					chart [idx].EndY = 1.0;
					idx++;
				}
				for (int x = 0; x < correlator.NumLines; x++) {
					chart [idx] = new Graph ();
					chart [idx].DotsPen = pens1 [x];
					chart [idx].Size = new Size (690, 200);
					if (x == 0) {
						chart [idx].Location = new Point (0, 200);
						chart [idx].LabelX = "Frequency";
						chart [idx].LabelY = "Occurrences";
						panel.Controls.Add (chart [idx]);
					} else {
						chart [idx].BackColor = Color.Transparent;
						chart [idx].ShowScale = false;
						chart [idx].Location = new Point (0, 0);
						chart [idx - 1].Controls.Add (chart [idx]);
					}
					chart [idx].StartX = 0;
					chart [idx].StartY = 0;
					chart [idx].EndX = 1.0;
					chart [idx].EndY = 1.0;
					idx++;
				}
				for (int x = 0; x < correlator.NumLines; x++) {
					chart [idx] = new Graph ();
					chart [idx].DotsPen = pens1 [x];
					chart [idx].Size = new Size (690, 200);
					if (x == 0) {
						chart [idx].Location = new Point (0, 400);
						chart [idx].LabelX = "delay (ns)";
						chart [idx].LabelY = "Auto-Correlations";
						panel.Controls.Add (chart [idx]);
					} else {
						chart [idx].BackColor = Color.Transparent;
						chart [idx].ShowScale = false;
						chart [idx].Location = new Point (0, 0);
						chart [idx - 1].Controls.Add (chart [idx]);
					}
					chart [idx].StartX = 0;
					chart [idx].EndX = correlator.DelaySize * 1000000000.0 / correlator.ClockFrequency;
					chart [idx].StartY = 0;
					chart [idx].EndY = 1.0;
					idx++;
				}
				int xx = 0;
				for (int x = 0; x < correlator.NumLines; x++) {
					for (int z = x + 1; z < correlator.NumLines; z++) {
						chart [idx] = new Graph ();
						chart [idx].Size = new Size (690, 200);
						if (xx == 0) {
							chart [idx].Location = new Point (0, 600);
							chart [idx].LabelX = "Time lag (ns)";
							chart [idx].LabelY = "Cross-Correlations";
							panel.Controls.Add (chart [idx]);
						} else {
							chart [idx].BackColor = Color.Transparent;
							chart [idx].ShowScale = false;
							chart [idx].Location = new Point (0, 0);
							chart [idx - 1].Controls.Add (chart [idx]);
						}
						chart [idx].DotsPen = pens2 [xx++];
						chart [idx].StartX = -(correlator.DelaySize * 1000000000.0 / correlator.ClockFrequency);
						chart [idx].EndX = (correlator.DelaySize * 1000000000.0 / correlator.ClockFrequency);
						chart [idx].StartY = 0;
						chart [idx].EndY = 1.0;
						idx++;
					}
				}
				this.Invoke ((MethodInvoker)delegate {
					panel.Invalidate(true);
				});
			}
			this.Resize += MainForm_Resize;
			this.FormClosing += MainForm_FormClosing;
		}

		void button_Click (object sender, EventArgs e)
		{
			Button btn = (Button)sender;
			start.Click -= button_Click;
			stop.Click -= button_Click;
			this.Invoke ((MethodInvoker)delegate {
				if (btn.Text == "Run") {
					for (int x = 0; x < chart.Length; x++) {
						chart [x].Dots.Clear ();
					}
					correlator.EnableCapture ();
					btn.Text = "Pause";
				} else if (btn.Text == "Pause") {
					correlator.DisableCapture ();
					btn.Text = "Continue";
				} else if (btn.Text == "Continue") {
					correlator.EnableCapture ();
					btn.Text = "Pause";
				} else if (btn.Text == "Stop") {
					correlator.DisableCapture ();
					start.Text = "Run";
				}
				panel.Invalidate(true);
				start.Invalidate (true);
			});
			start.Click += button_Click;
			stop.Click += button_Click;
		}

		void Scalex_ValueChanged (object sender, EventArgs e)
		{
			NumericUpDown updown = (NumericUpDown)sender;
			updown.ValueChanged -= Scalex_ValueChanged;
			if (chart != null) {
				int idx = 0;
				for (int x = 0; x < correlator.NumLines; x++) {
					chart [idx].EndX = TimeScale / (double)updown.Value;
					chart [idx + correlator.NumLines * 2].EndX = (correlator.DelaySize * 1000000000.0 / correlator.ClockFrequency) / (double)updown.Value;
					idx++;
				}
				idx += correlator.NumLines * 2;
				for (int x = 0; x < correlator.NumBaselines; x++) {
					chart [idx].StartX = -(correlator.DelaySize * 1000000000.0 / correlator.ClockFrequency) / (double)updown.Value;
					chart [idx].EndX = (correlator.DelaySize * 1000000000.0 / correlator.ClockFrequency) / (double)updown.Value;
					idx++;
				}
			}
			updown.ValueChanged += Scalex_ValueChanged;
		}

		void MainForm_FormClosing (object sender, FormClosingEventArgs e)
		{
			for (int x = 0; x < correlator.NumLines; x++) {
				correlator.SetLine (x, false, false);
			}
			correlator.DisableCapture ();
			correlator.Disconnect ();
		}

		void MainForm_Resize (object sender, EventArgs e)
		{
			this.Resize -= MainForm_Resize;
			panel.Size = new Size (this.ClientSize.Width, this.ClientSize.Height - (33 + 30 * (correlator.NumLines / 4)));
			for (int x = 0; x < chart.Length; x++) {
				chart [x].Size = new Size (this.ClientSize.Width-30, 200);
			}
			this.Invoke ((MethodInvoker)delegate {
				panel.Invalidate(true);
			});

			this.Resize += MainForm_Resize;
		}

		void Line_SelectedIndexChanged (object sender, EventArgs e)
		{
			ComboBox combo = (ComboBox)sender;
			combo.SelectedIndexChanged -= Line_SelectedIndexChanged;
			if(combo.Name.Length > 4)
				correlator.SetLine (Int32.Parse (combo.Name.Substring (4)), combo.SelectedItem.ToString().Contains("On"), combo.SelectedItem.ToString().Contains("Power"));
			combo.SelectedIndexChanged += Line_SelectedIndexChanged;
		}

		void Baudrate_SelectedIndexChanged (object sender, EventArgs e)
		{
			ComboBox combo = (ComboBox)sender;
			combo.SelectedIndexChanged -= Baudrate_SelectedIndexChanged;
			correlator.SetBaudRate ((baud_rate)combo.SelectedIndex);
			combo.SelectedIndexChanged += Baudrate_SelectedIndexChanged;
		}

		void Freqdiv_ValueChanged (object sender, EventArgs e)
		{
			NumericUpDown updown = (NumericUpDown)sender;
			updown.ValueChanged -= Freqdiv_ValueChanged;
			correlator.SetFrequencyDivider ((byte)updown.Value);
			for (int idx = 0; idx < correlator.NumLines; idx++) {
				chart [idx + correlator.NumLines * 2].StartX = 0;
				chart [idx + correlator.NumLines * 2].EndX = (correlator.DelaySize * 1000000000.0 / correlator.ClockFrequency);
				this.Invoke ((MethodInvoker)delegate {
					chart [idx + correlator.NumLines * 2].Invalidate(false);
				});
			}
			for (int idx = 0; idx < correlator.NumBaselines; idx++) {
				chart [idx + correlator.NumLines * 3].StartX = -(correlator.DelaySize * 1000000000.0 / correlator.ClockFrequency);
				chart [idx + correlator.NumLines * 3].EndX = (correlator.DelaySize * 1000000000.0 / correlator.ClockFrequency);
				this.Invoke ((MethodInvoker)delegate {
					chart [idx + correlator.NumLines * 3].Invalidate(false);
				});
			}
			updown.ValueChanged += Freqdiv_ValueChanged;
		}

		void MainForm_Shown (object sender, EventArgs e)
		{
			Label label;
			label = new Label ();
			label.Size = new Size (75, 23);
			label.Location = new Point (5, 3);
			label.Text = "Serial port";
			Controls.Add (label);
			ComboBox port = new ComboBox ();
			port.Size = new Size (75, 23);
			port.Location = new Point (80, 3);
			port.Items.AddRange (SerialPort.GetPortNames ());
			port.SelectedIndex = 0;
			port.SelectedIndexChanged += Port_SelectedIndexChanged;
			Controls.Add (port);
		}

		void Port_SelectedIndexChanged (object sender, EventArgs e)
		{
			ComboBox combo = (ComboBox)sender;
			combo.SelectedIndexChanged -= Line_SelectedIndexChanged;
			correlator.Connect (combo.SelectedItem.ToString ());
			combo.SelectedIndexChanged += Line_SelectedIndexChanged;
		}

		double[][]counts, lines;

		void Correlator_SweepUpdate(object sender, SweepUpdateEventArgs e)
		{
			correlator.SweepUpdate -= Correlator_SweepUpdate;
			if (chart == null || e == null)
				return;
			if (e.Mode == OperatingMode.Crosscorrelator) {
				int idx = e.Index + correlator.NumLines * 3;
				try {
					if (chart [idx] == null)
						return;
					var values = e.Counts;
					Parallel.For (0, correlator.DelaySize * 2, delegate(int y) {
						try {
							double time = (double)(y - correlator.DelaySize) * 1000000000.0 / correlator.ClockFrequency;
							if (chart [idx].Dots.ContainsKey (time))
								chart [idx].Dots [time] = values [y];
							else
								chart [idx].Dots.Add (time, values [y]);
						} catch {
						}
					});
				} catch {
				}
			} else if (e.Mode == OperatingMode.Autocorrelator) {
				int idx = e.Index + correlator.NumLines * 2;
				try {
					if (chart [idx] == null)
						return;
					chart [idx].StartX = 0;
					var values = e.Counts;
					Parallel.For (0, values.Count, delegate(int y) {
						try {
							double time = (double)y * 1000000000.0 / correlator.ClockFrequency;
							if (chart [idx].Dots.ContainsKey (time))
								chart [idx].Dots [time] = values [y];
							else
								chart [idx].Dots.Add (time, values [y]);
						} catch {
						}
					});
				} catch {
				}
			} else if (e.Mode == OperatingMode.Counter) {
				int idx = e.Index;
				try {
					if (chart [idx] == null)
						return;
					chart [idx].StartX = 0;
					var values = e.Counts;
					Parallel.For (0, (int)(TimeScale / correlator.PacketTime), delegate(int y) {
						try {
							double time = (double)y * correlator.PacketTime;
							if (chart [idx].Dots.ContainsKey (time))
								chart [idx].Dots [time] = values [values.Count - 1 - y];
							else
								chart [idx].Dots.Add (time, values [values.Count - 1 - y]);
						} catch {
						}
					});
				} catch {
				}
				idx += correlator.NumLines;
				try {
					if (chart [idx] == null)
						return;
					chart [idx].StartX = chart [idx - correlator.NumLines].Dots.Values.Min ();
					chart [idx].EndX = chart [idx - correlator.NumLines].Dots.Values.Max ();
					var vals = chart [idx - correlator.NumLines].Dots.Values.ToArray ().ToList ();
					vals.Sort ();
					chart [idx].Dots.Clear ();
					Parallel.For (0, vals.Count, delegate(int z) {
						try {
							if (vals [z] > 0) {
								if (chart [idx].Dots.ContainsKey (vals [z]))
									chart [idx].Dots [vals [z]]++;
								else
									chart [idx].Dots.Add (vals [z], 1);
							}
						} catch {
						}
					});
				} catch {
				}
			}
			try {
				for (int idx = 0; idx < chart.Length; idx++) {
					try {
						double diff = chart [idx].Dots.Values.Max () - chart [idx].Dots.Values.Min () * 0.2;
						chart [idx].EndY = chart [idx].Dots.Values.Max () + diff;
						chart [idx].StartY = chart [idx].Dots.Values.Min () - diff;
					} catch {
					}
				}
				int xx = 0;
				double ysdiff = chart [0].StartY;
				double yediff = chart [0].EndY;

				for (int x = 0; x < correlator.NumLines; x++, xx++) {
					ysdiff = Math.Min (ysdiff, chart [xx].StartY);
					yediff = Math.Max (yediff, chart [xx].EndY);
				}
				for (int x = xx - correlator.NumLines; x < xx; x++) {
					chart [x].StartY = ysdiff;
					chart [x].EndY = yediff;
				}

				double xsdiff = chart [xx].StartX;
				double xediff = chart [xx].EndX;
				ysdiff = chart [xx].StartY;
				yediff = chart [xx].EndY;

				for (int x = 0; x < correlator.NumLines; x++, xx++) {
					xsdiff = Math.Min (xsdiff, chart [xx].StartX);
					xediff = Math.Max (xediff, chart [xx].EndX);
					ysdiff = Math.Min (ysdiff, chart [xx].StartY);
					yediff = Math.Max (yediff, chart [xx].EndY);
				}
				for (int x = xx - correlator.NumLines; x < xx; x++) {
					chart [x].StartX = xsdiff;
					chart [x].EndX = xediff;
					chart [x].StartY = ysdiff;
					chart [x].EndY = yediff;
				}

				ysdiff = chart [xx].StartY;
				yediff = chart [xx].EndY;

				for (int x = 0; x < correlator.NumLines; x++, xx++) {
					ysdiff = Math.Min (ysdiff, chart [xx].StartY);
					yediff = Math.Max (yediff, chart [xx].EndY);
				}
				for (int x = xx - correlator.NumLines; x < xx; x++) {
					chart [x].StartY = ysdiff;
					chart [x].EndY = yediff;
				}

				ysdiff = chart [xx].StartY;
				yediff = chart [xx].EndY;

				for (int x = 0; x < correlator.NumBaselines; x++, xx++) {
					ysdiff = Math.Min (ysdiff, chart [xx].StartY);
					yediff = Math.Max (yediff, chart [xx].EndY);
				}
				for (int x = xx - correlator.NumBaselines; x < xx; x++) {
					chart [x].StartY = ysdiff;
					chart [x].EndY = yediff;
				}
				this.Invoke ((MethodInvoker)delegate {
					panel.Invalidate (true);
				});
			} catch {
			}
			correlator.SweepUpdate += Correlator_SweepUpdate;
		}
	}
}
