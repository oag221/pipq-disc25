import plotly.graph_objects as go #from plotly import graph_objs as go
from plot_util import *
from plotly.subplots import make_subplots
import math
from absl import app
from absl import flags
import os

# confused about the "./microbench" vs "microbench"
# confused in "make_csv.sh" about where the csv should be created, bc when it makes the second one it thinks the first csv
#   is a data structure --> maybe it's fine and just ignored, but makes me think there could be something wrong here
# NOTES
# pass in many update rates, one RQ rate

FLAGS = flags.FLAGS

# Whether or not to plot the microbenchmark and where to find the data.
flags.DEFINE_bool("microbench", False, "Plot microbenchmark results")
flags.DEFINE_string(
    "microbench_dir",
    "./microbench/data",
    "Location of microbenchmark data. If the folder corresponding to each experiment does not contain a .csv file, it will be automatically generated",
)

flags.DEFINE_string(
    "sssp_dir",
    "./sssp/data",
    "Location of sssp data. If the folder corresponding to each experiment does not contain a .csv file, it will be automatically generated",
)

# type of experiment - "micro", "phased", "desg"
flags.DEFINE_string(
    "exp_type",
    "micro",
    "Type of experiment to perform",
)

flags.DEFINE_list(
    "desg_tds",
    [4,8,48],
    "Number of desg del-min threads",
)

# Whether or not to plot the macrobenchmark and where to find the data.
flags.DEFINE_bool("macrobench", False, "Plot macrobenchmark results")
flags.DEFINE_string(
    "macrobench_dir",
    "./macrobench/data",
    "Location of macrobenchmark data. If the folder corresponding to each experiment does not contain a .csv file, it will be automatically generated",
)

# Whether or not to save data as interactive HTML files and where to save it.
flags.DEFINE_bool("save_plots", False, "Save plots as interactive HTML files")
flags.DEFINE_string("save_dir", "./figures", "Directory where to save plots")

# Whether or not to include speedup information in output.
flags.DEFINE_bool("print_speedup", False, "Print the speedup over unsafe")

# Flags related to automatic config detection.
flags.DEFINE_string(
    "generate_script",
    "microbench/experiment_list_generate.sh",
    "Script used to generate experiments that is examined when detecting the configuration.",
)
flags.DEFINE_string(
    "runscript",
    "microbench/run_script_updated.sh",
    "Script used to run experiments that includes additional configuration info.",
)
flags.DEFINE_bool(
    "autodetect",
    True, # TODO: do I want this to be true? - verify that files I have modified can be 
    "Automatically derives all plot configuration information from the config files",
)
flags.DEFINE_bool(
    "detect_threads",
    False,
    "Automatically detects the maximum number of threads and thread incrment from config.mk",
)
flags.DEFINE_bool(
    "detect_experiments",
    False,
    "Automatically pull data structure and max key configurations from experiment_list_generate.sh",
)
flags.DEFINE_bool(
    "detect_trials",
    False,
    "Automatically pull number of trials information from runscript.sh",
)

flags.DEFINE_integer(
    "workloads_rqrate",
    10,
    "Rate of range query operations to use when plotting the 'workloads' experiment",
)
flags.DEFINE_list(
    "workloads_urates",
    [0, 50, 95, 100],
    "Rate of range query operations to use when plotting the 'workloads' experiment",
)
flags.DEFINE_integer(
    "rqsizes_numrqthreads",
    24,
    "Number of dedicated RQ threads used in the 'rqthreads' experiment",
)
flags.DEFINE_list(
    "rqsizes_rqsizes",
    [8, 64, 256, 1024, 8092, 16184],
    "Range query sizes to be used in the 'rqthreads' experiment",
)

flags.DEFINE_list("experiments", None, "List of experiments to plot")
flags.DEFINE_list("datastructures", None, "List of data structures to plot")
flags.DEFINE_list("max_keys", None, "List of max keys to use while plotting")
flags.DEFINE_list("nthreads", None, "List of thread counts to plot")
flags.DEFINE_integer(
    "ntrials", 5,
    "Number of trials per experiment (used for averaging results)")

flags.DEFINE_bool("legends", True, "Whether to show legends in the plots")
flags.DEFINE_bool("yaxis_titles", True,
                  "Whether to include y-axis titles in the plots")


# TODO: diff function for other types of plots?
def plot_workload(
    dirpath,
    exp_type,
    max_key,
    ins_rate, # for phased it's the phased string
    desg_tds,
    threads,
    ntrials,
    ylabel=False,
    legend=False,
    save=False,
    save_dir="",
):
    """ Generates a plot showing throughput as a function of number of threads
        for the given data structure. 
        
    Arguments:
        dirpath: A string indicating where the data to plot lives.
        exp_type: the type of experiment ("micro", "phased", etc.)
        max_key: The configured size for the run to plot.
        ins_rate: Update rate of the run to plot.
        rq_rate: RQ rate of the run to plot.
        threads: An array of thread counts for the x-axis.
        ntrials: Number of trials used to generate data.
        ylabel: Whether or not to include label on y-axis
        legend: Whether or not to include legend.
        save: Whether or not to save plots to disk.
        save_dir: Where plots are saved to.
    """
    reset_base_config()
    #path = os.path.join(dirpath, "workloads")
    csvfile = CSVFile.get_or_gen_csv(dirpath, exp_type, ntrials)
    csv = CSVFile(csvfile)
    
    # Provide column labels for desired x and y axis
    x_axis = "threads"
    y_axis = "tot_thruput"
    error_rate = "std_dev"

    # Init data store
    data = {}

    # Ignores rows in .csv with the following label
    ignore = ["ubundle"]
    data_structs = [k for k in plotconfig.keys() if k not in ignore]

    # Read in data for each algorithm
    phased_str = ""
    if exp_type == "micro":
        print("Getting data:, max_key={}, ins_rate={}".format(
                max_key, ins_rate))
    elif exp_type == "desg":
        print("Getting data:, max_key={}, desg_tds={}".format(
                max_key, desg_tds))
    elif exp_type == "phased":
        print("Getting data:, max_key={} for phased experiment {}".format(
                max_key, ins_rate))
        phased_str = ins_rate
    
    if exp_type == "micro":
        data = csv.getdata(["max_key", "ins_rate"],
                        [max_key, ins_rate]) # gets all data from the rows in first array which match the workload in the second
    elif exp_type == "desg":
        data = csv.getdata(["max_key", "desg_tds"],
                        [max_key, desg_tds])
    elif exp_type == "phased":
        data = csv.getdata(["max_key", "phase_str"], # get all data
                        [max_key, phased_str])
    data[y_axis] = data[y_axis] / 1000000
    data[error_rate] = data[error_rate] / 1000000

    if data.empty:
        report_empty("max_key={}, ins_rate={}".format(
            max_key, ins_rate))
        print("No data.")
        return  # If no data to plot, then don't

    # Plot layout configuration.
    x_axis_layout_["title"] = None
    x_axis_layout_["tickfont"]["size"] = 52
    x_axis_layout_["nticks"] = len(threads)
    if ylabel:
        y_axis_layout_["title"]["text"] = "Mops/s"
        y_axis_layout_["title"]["font"]["size"] = 50
        y_axis_layout_["title"]["standoff"] = 50
    else:
        y_axis_layout_["title"] = None
    y_axis_layout_["tickfont"]["size"] = 50
    y_axis_layout_["nticks"] = 5
    legend_layout_ = (
        # {"font": legend_font_, "orientation": "h", "x": 0, "y": 1} if legend else {}
        {
            "font": legend_font_,
            "orientation": "v",
            "x": 1.1,
            "y": 1
        } if legend else {})
    layout_["legend"] = legend_layout_
    layout_["autosize"] = False
    layout_["width"] = 800 if legend else 560
    layout_["height"] = 450

    fig = go.Figure(layout=layout_)
    
    max_co_var = 0
    tot_co_var = 0
    count = 0
    for a in data_structs:
        symbol_ = plotconfig[a]["symbol"]
        color_ = update_opacity(plotconfig[a]["color"], 1)
        marker_ = {
            "symbol": symbol_,
            "color": color_,
            "size": 25,
            "line": {
                "width": (2 if legend else 5),
                "color": "black"
            },
        }
        line_ = {"width": 7}
        
        name_ = "<b>" + plotconfig[a]["label"] + "</b>"
        
        y_1 = data[data["list"] == a] # only choose elements of correct ds 
        y_ = y_1[y_1.threads.isin(threads)]["tot_thruput"]

        print(y_1)
        print("\n")
        print(y_)
        print("\n\n")

        # also grab the standard deviation, which I added (see make_csv.sh)
        error = y_1[y_1.threads.isin(threads)]["std_dev"]

        error_list = error.tolist()
        y_list = y_.tolist()
        for i in range(len(error_list)):
            temp_co_var = error_list[i] / y_list[i]
            tot_co_var += temp_co_var
            count += 1
            #print("\tTemp Co-Var: ", temp_co_var, "\t (", threads[i], ")")
            if temp_co_var > max_co_var:
                max_co_var = temp_co_var

        fig.add_scatter(
            x=threads,
            y=y_,
            name=name_,
            marker=marker_,
            line=line_,
            showlegend=legend,
            error_y=dict(
                type='data', # value of error bar - standard deviation
                array=error,
                visible=True),
        )

    print("Maximum coefficient variance: ", max_co_var)
    print("Total coefficient variance: ", tot_co_var)
    print("Count: ", count)

    if not save:
        fig.show()
    else:
        save_dir = os.path.join(save_dir, exp_type)
        os.makedirs(save_dir, exist_ok=True)
        filename = ""
        if exp_type == "phased":
            filename = ("phased_" + str(phased_str) + ".html") # ins_rate is phased string
        elif exp_type == "desg":
            filename = ("update" + str(ins_rate) + "_maxkey" +
                    str(max_key) + "_desg-tds" + str(desg_tds) + ".html")
        else:
            filename = ("update" + str(ins_rate) + "_maxkey" +
                    str(max_key) + ".html")
        fig.write_html(os.path.join(save_dir, filename))

def plot_desg_sep(
    dirpath,
    exp_type,
    max_key,
    desg_tds,
    threads,
    ntrials,
    ylabel=False,
    legend=False,
    save=False,
    save_dir="",
):
    """ Generates a plot showing throughput as a function of number of threads
        for the given data structure. 
        
    Arguments:
        dirpath: A string indicating where the data to plot lives.
        exp_type: whether to plot ins thpt ("ins") or del-min thpt ("del-min")
        max_key: The configured size for the run to plot.
        desg_tds: number of designated threads performing del-min
        threads: An array of thread counts for the x-axis.
        ntrials: Number of trials used to generate data.
        ylabel: Whether or not to include label on y-axis
        legend: Whether or not to include legend.
        save: Whether or not to save plots to disk.
        save_dir: Where plots are saved to.
    """
    threads = [12,24,36,48,60,72,84,96]
    reset_base_config()
    csvfile = CSVFile.get_or_gen_csv(dirpath, "desg", ntrials)
    csv = CSVFile(csvfile)
    
    # Provide column labels for desired x and y axis
    y_label = ""
    if exp_type == "ins":
        y_label = "ins_thruput"
    else:
        y_label = "del_thruput"
    error_rate = "std_dev"

    # Init data store
    data = {}

    # Ignores rows in .csv with the following label
    ignore = ["ubundle"]
    data_structs = [k for k in plotconfig.keys() if k not in ignore]

    # Read in data for each algorithm
    
    print("Getting data:, max_key={}, thpt={}, desg_tds={}".format(max_key, exp_type, desg_tds))
    
    data = csv.getdata(["max_key", "desg_tds"],
                        [max_key, desg_tds])
    # elif exp_type == "phased": todo
    #     data = csv.getdata(["max_key", "desg_tds"],
    #                     [max_key, phased_])
    data[y_label] = data[y_label] / 1000000
    data[error_rate] = data[error_rate] / 1000000

    if data.empty:
        report_empty("max_key={}, ins_rate={}".format(
            max_key, ins_rate))
        print("No data.")
        return  # If no data to plot, then don't

    # Plot layout configuration.
    x_axis_layout_["title"] = None
    x_axis_layout_["tickfont"]["size"] = 52
    x_axis_layout_["nticks"] = len(threads)
    if ylabel:
        y_axis_layout_["title"]["text"] = "Mops/s"
        y_axis_layout_["title"]["font"]["size"] = 50
        y_axis_layout_["title"]["standoff"] = 50
    else:
        y_axis_layout_["title"] = None
    y_axis_layout_["tickfont"]["size"] = 50
    y_axis_layout_["nticks"] = 5
    legend_layout_ = (
        # {"font": legend_font_, "orientation": "h", "x": 0, "y": 1} if legend else {}
        {
            "font": legend_font_,
            "orientation": "v",
            "x": 1.1,
            "y": 1
        } if legend else {})
    layout_["legend"] = legend_layout_
    layout_["autosize"] = False
    layout_["width"] = 800 if legend else 560
    layout_["height"] = 450

    fig = go.Figure(layout=layout_)
    
    max_co_var = 0
    tot_co_var = 0
    count = 0
    for a in data_structs:
        symbol_ = plotconfig[a]["symbol"]
        color_ = update_opacity(plotconfig[a]["color"], 1)
        marker_ = {
            "symbol": symbol_,
            "color": color_,
            "size": 25,
            "line": {
                "width": (2 if legend else 5),
                "color": "black"
            },
        }
        line_ = {"width": 7}
        
        name_ = "<b>" + plotconfig[a]["label"] + "</b>"
        
        y_1 = data[data["list"] == a] # only choose elements of correct ds 
        y_ = y_1[y_1.threads.isin(threads)][y_label]

        fig.add_scatter(
            x=threads,
            y=y_,
            name=name_,
            marker=marker_,
            line=line_,
            showlegend=legend
        )
        count += 1
    print("Count: ", count)

    if not save:
        fig.show()
    else:
        save_dir = os.path.join(save_dir, "desg-sep")
        os.makedirs(save_dir, exist_ok=True)
        filename = ("update0" + "_maxkey" + str(max_key) +
                    "_desg-tds" + str(desg_tds) + "_desg-tds-type" + str(exp_type) + ".html")
        fig.write_html(os.path.join(save_dir, filename))

def plot_lat_sep(
    dirpath,
    exp_type,
    max_key,
    ins_rate,
    threads,
    ntrials,
    ylabel=False,
    legend=False,
    save=False,
    save_dir="",
):
    """ Generates a plot showing throughput as a function of number of threads
        for the given data structure. 
        
    Arguments:
        dirpath: A string indicating where the data to plot lives.
        exp_type: whether to plot ins thpt ("ins") or del-min thpt ("del-min")
        max_key: The configured size for the run to plot.
        desg_tds: number of designated threads performing del-min
        threads: An array of thread counts for the x-axis.
        ntrials: Number of trials used to generate data.
        ylabel: Whether or not to include label on y-axis
        legend: Whether or not to include legend.
        save: Whether or not to save plots to disk.
        save_dir: Where plots are saved to.
    """
    reset_base_config()
    csvfile = CSVFile.get_or_gen_csv(dirpath, "latency", ntrials)
    csv = CSVFile(csvfile)

    if ins_rate == 0 and exp_type == "ins":
        return
    if ins_rate == 100 and exp_type == "del-min":
        return
    
    # Provide column labels for desired x and y axis
    y_label = ""
    if exp_type == "ins":
        y_label = "ins_lat"
    else:
        y_label = "del_lat"

    # Init data store
    data = {}

    # Ignores rows in .csv with the following label
    ignore = ["ubundle"]
    data_structs = [k for k in plotconfig.keys() if k not in ignore]

    # Read in data for each algorithm
    
    print("Getting data:, max_key={}, ins_rate={}".format(max_key, ins_rate))
    
    data = csv.getdata(["max_key", "ins_rate"],
                        [max_key, ins_rate])
    # elif exp_type == "phased": todo
    #     data = csv.getdata(["max_key", "desg_tds"],
    #                     [max_key, phased_])

    # nanoseconds -> microseconds
    data[y_label] = data[y_label] / 1000

    if data.empty:
        report_empty("max_key={}, ins_rate={}".format(
            max_key, ins_rate))
        print("No data.")
        return  # If no data to plot, then don't

    # Plot layout configuration.
    x_axis_layout_["title"] = None
    x_axis_layout_["tickfont"]["size"] = 52
    x_axis_layout_["nticks"] = len(threads)
    if ylabel:
        y_axis_layout_["title"]["text"] = "Microseconds"
        y_axis_layout_["title"]["font"]["size"] = 50
        y_axis_layout_["title"]["standoff"] = 50
    else:
        y_axis_layout_["title"] = None
    y_axis_layout_["tickfont"]["size"] = 50
    y_axis_layout_["nticks"] = 5
    legend_layout_ = (
        # {"font": legend_font_, "orientation": "h", "x": 0, "y": 1} if legend else {}
        {
            "font": legend_font_,
            "orientation": "v",
            "x": 1.1,
            "y": 1
        } if legend else {})
    layout_["legend"] = legend_layout_
    layout_["autosize"] = False
    layout_["width"] = 800 if legend else 560
    layout_["height"] = 450

    fig = go.Figure(layout=layout_)
    
    max_co_var = 0
    tot_co_var = 0
    count = 0
    for a in data_structs:
        symbol_ = plotconfig[a]["symbol"]
        color_ = update_opacity(plotconfig[a]["color"], 1)
        marker_ = {
            "symbol": symbol_,
            "color": color_,
            "size": 25,
            "line": {
                "width": (2 if legend else 5),
                "color": "black"
            },
        }
        line_ = {"width": 7}
        
        name_ = "<b>" + plotconfig[a]["label"] + "</b>"
        
        y_1 = data[data["list"] == a] # only choose elements of correct ds 
        y_ = y_1[y_1.threads.isin(threads)][y_label]

        fig.add_scatter(
            x=threads,
            y=y_,
            name=name_,
            marker=marker_,
            line=line_,
            showlegend=legend
        )
        count += 1
    print("Count: ", count)

    if not save:
        fig.show()
    else:
        save_dir = os.path.join(save_dir, "latency")
        os.makedirs(save_dir, exist_ok=True)
        filename = ("update0" + "_maxkey" + str(max_key) +
                    "_ins" + str(ins_rate) + "_latency_" + str(exp_type) + ".html")
        fig.write_html(os.path.join(save_dir, filename))

def plot_sssp(
    dirpath,
    exp_type,
    graph,
    threads,
    ntrials,
    ylabel=False,
    legend=False,
    save=False,
    save_dir="",
):
    """ Generates a plot showing throughput as a function of number of threads
        for the given data structure. 
        
    Arguments:
        dirpath: A string indicating where the data to plot lives - sssp/data
        exp_type: sssp
        graph: the input graph
        threads: An array of thread counts for the x-axis.
        ntrials: Number of trials used to generate data.
        ylabel: Whether or not to include label on y-axis
        legend: Whether or not to include legend.
        save: Whether or not to save plots to disk.
        save_dir: Where plots are saved to.
    """
    reset_base_config()
    csvfile = CSVFile.get_or_gen_csv_sssp(dirpath, "sssp", ntrials)
    csv = CSVFile(csvfile)
    
    # Provide column labels for desired x and y axis
    y_label = "duration"

    # Init data store
    data = {}

    # Ignores rows in .csv with the following labels
    ignore = ["ubundle"]
    data_structs = [k for k in plotconfig.keys() if k not in ignore]

    # mapping config to value in csv (see thread_data.h in sssp/include/)
    d = {
        "Linden": "Linden",
        "Spray": "SprayList",
        "Lotan": "Lotan-Shavit",
        "SMQ": "SMQ",
        "Us-Lin": "Numa-PQ",
        "Us-Rel": "Numa-PQ-4"
    }

    # Read in data for each algorithm
    
    print("Getting data:, graph={}".format(graph))
    
    data = csv.getdata(["graph"],[graph])
    data[y_label] = data[y_label] / 1000 # convert ms to seconds
    print(f"data: {data}")

    if data.empty:
        report_empty("graph={}".format(graph))
        print("No data.")
        return  # If no data to plot, then don't

    # Plot layout configuration.
    x_axis_layout_["title"] = None
    x_axis_layout_["tickfont"]["size"] = 52
    x_axis_layout_["nticks"] = len(threads)
    if ylabel:
        y_axis_layout_["title"]["text"] = "Seconds"
        y_axis_layout_["title"]["font"]["size"] = 50
        y_axis_layout_["title"]["standoff"] = 50
    else:
        y_axis_layout_["title"] = None
    y_axis_layout_["tickfont"]["size"] = 50
    y_axis_layout_["nticks"] = 5
    legend_layout_ = (
        # {"font": legend_font_, "orientation": "h", "x": 0, "y": 1} if legend else {}
        {
            "font": legend_font_,
            "orientation": "v",
            "x": 1.1,
            "y": 1
        } if legend else {})
    layout_["legend"] = legend_layout_
    layout_["autosize"] = False
    layout_["width"] = 800 if legend else 560
    layout_["height"] = 450

    fig = go.Figure(layout=layout_)
    count = 0
    for a in data_structs:
        symbol_ = plotconfig[a]["symbol"]
        color_ = update_opacity(plotconfig[a]["color"], 1)
        marker_ = {
            "symbol": symbol_,
            "color": color_,
            "size": 25,
            "line": {
                "width": (2 if legend else 5),
                "color": "black"
            },
        }
        line_ = {"width": 7}
        
        name_ = "<b>" + plotconfig[a]["label"] + "</b>"
        
        ds_csv_name = d[a]
        print(f"ds_csv_name = {ds_csv_name}")
        y_1 = data[data["ds_name"] == ds_csv_name] # only choose elements of correct ds 
        y_ = y_1[y_1.threads.isin(threads)][y_label]

        fig.add_scatter(
            x=threads,
            y=y_,
            name=name_,
            marker=marker_,
            line=line_,
            showlegend=legend
        )
        count += 1
    print("Count: ", count)

    if not save:
        fig.show()
    else:
        save_dir = os.path.join(save_dir, "sssp")
        os.makedirs(save_dir, exist_ok=True)
        filename = ("sssp_" + str(graph) + ".html")
        fig.write_html(os.path.join(save_dir, filename))

def plot_desg_usonly(
    dirpath,
    exp_type,
    max_key,
    desg_tds,
    threads,
    ntrials,
    ylabel=False,
    legend=False,
    save=False,
    save_dir="",
):
    """ Generates a plot showing throughput as a function of number of threads
        for the given data structure. 
        
    Arguments:
        dirpath: A string indicating where the data to plot lives.
        exp_type: whether to plot ins thpt ("ins") or del-min thpt ("del-min")
        max_key: The configured size for the run to plot.
        desg_tds: number of designated threads performing del-min
        threads: An array of thread counts for the x-axis.
        ntrials: Number of trials used to generate data.
        ylabel: Whether or not to include label on y-axis
        legend: Whether or not to include legend.
        save: Whether or not to save plots to disk.
        save_dir: Where plots are saved to.
    """
    reset_base_config()
    csvfile = CSVFile.get_or_gen_csv(dirpath, "desg", ntrials)
    csv = CSVFile(csvfile)
    
    # Provide column labels for desired x and y axis
    y_label = ""
    if exp_type == "ins":
        y_label = "ins_thruput"
    else:
        y_label = "del_thruput"
    error_rate = "std_dev"

    # Init data store
    data = {}

    # Ignores rows in .csv with the following label
    ignore = ["ubundle"]
    desg_num = [k for k in plotconfig_desg_us.keys() if k not in ignore]

    # Read in data for each algorithm
    
    print("Getting data:, max_key={}, thpt={}, desg_tds={}".format(max_key, exp_type, desg_tds))
    
    data = csv.getdata(["max_key"],[max_key]) # get all data
    
    data[y_label] = data[y_label] / 1000000
    data[error_rate] = data[error_rate] / 1000000

    if data.empty:
        report_empty("max_key={}".format(
            max_key))
        print("No data.")
        return  # If no data to plot, then don't

    # Plot layout configuration.
    x_axis_layout_["title"] = None
    x_axis_layout_["tickfont"]["size"] = 52
    x_axis_layout_["nticks"] = len(threads)
    if ylabel:
        y_axis_layout_["title"]["text"] = "Mops/s"
        y_axis_layout_["title"]["font"]["size"] = 50
        y_axis_layout_["title"]["standoff"] = 50
    else:
        y_axis_layout_["title"] = None
    y_axis_layout_["tickfont"]["size"] = 50
    y_axis_layout_["nticks"] = 5
    legend_layout_ = (
        # {"font": legend_font_, "orientation": "h", "x": 0, "y": 1} if legend else {}
        {
            "font": legend_font_,
            "orientation": "v",
            "x": 1.1,
            "y": 1
        } if legend else {})
    layout_["legend"] = legend_layout_
    layout_["autosize"] = False
    layout_["width"] = 800 if legend else 560
    layout_["height"] = 450

    fig = go.Figure(layout=layout_)
    
    max_co_var = 0
    tot_co_var = 0
    count = 0
    for a in desg_num:
        symbol_ = plotconfig_desg_us[a]["symbol"]
        color_ = update_opacity(plotconfig_desg_us[a]["color"], 1)
        marker_ = {
            "symbol": symbol_,
            "color": color_,
            "size": 25,
            "line": {
                "width": (2 if legend else 5),
                "color": "black"
            },
        }
        line_ = {"width": 7}
        
        name_ = "<b>" + plotconfig_desg_us[a]["label"] + "</b>"
        
        y_1 = data[data["desg_tds"] == a] # only choose elements of correct ds 
        y_ = y_1[y_1.threads.isin(threads)][y_label]

        fig.add_scatter(
            x=threads,
            y=y_,
            name=name_,
            marker=marker_,
            line=line_,
            showlegend=legend
        )
        count += 1
    print("Count: ", count)

    if not save:
        fig.show()
    else:
        save_dir = os.path.join(save_dir, "desg-usonly")
        os.makedirs(save_dir, exist_ok=True)
        filename = ("desg_usonly_" + str(exp_type) + ".html")
        fig.write_html(os.path.join(save_dir, filename))

def plot_paths(
    dirpath,
    exp_type,
    max_key,
    ins_rate,
    threads,
    ntrials,
    ylabel=False,
    legend=False,
    save=False,
    save_dir="",
):
    """ Generates a plot showing throughput as a function of number of threads
        for the given data structure. 
        
    Arguments:
        dirpath: A string indicating where the data to plot lives.
        exp_type: whether to plot ins thpt ("ins") or del-min thpt ("del-min")
        max_key: The configured size for the run to plot.
        desg_tds: number of designated threads performing del-min
        threads: An array of thread counts for the x-axis.
        ntrials: Number of trials used to generate data.
        ylabel: Whether or not to include label on y-axis
        legend: Whether or not to include legend.
        save: Whether or not to save plots to disk.
        save_dir: Where plots are saved to.
    """
    reset_base_config()
    csvfile = CSVFile.get_or_gen_csv(dirpath, "paths", ntrials)
    csv = CSVFile(csvfile)

    # Init data store
    data = {}
    categories = [1, 24, 48, 72, 96]
    
    print("Getting data:, max_key={}, exp={}, ins_rate={}".format(max_key, exp_type, ins_rate))
    
    data = csv.getdata(["max_key", "ins_rate"],[max_key, ins_rate]) # get all data

    if data.empty:
        report_empty("max_key={}".format(
            max_key))
        print("No data.")
        return  # If no data to plot, then don't

    # Plot layout configuration.
    x_axis_layout_["title"] = None
    x_axis_layout_["tickfont"]["size"] = 52
    x_axis_layout_["nticks"] = len(categories)

    y_axis_layout_["title"]["text"] = "% Path Followed"
    y_axis_layout_["title"]["font"]["size"] = 40
    y_axis_layout_["title"]["standoff"] = 50
    y_axis_layout_["range"] = [0, 100]
    y_axis_layout_["tickvals"] = [0, 25, 50, 75, 100]

    y_axis_layout_["tickfont"]["size"] = 50
    legend_layout_ = (
        # {"font": legend_font_, "orientation": "h", "x": 0, "y": 1} if legend else {}
        {
            "font": legend_font_,
            "orientation": "v",
            "x": 1.1,
            "y": 1
        })
    layout_["legend"] = legend_layout_
    layout_["autosize"] = False
    layout_["width"] = 800
    layout_["height"] = 450
    layout_["barmode"] = 'stack'

    fig = go.Figure(layout=layout_)
    
    fast = data[data.threads.isin(categories)]["fast"]
    slower = data[data.threads.isin(categories)]["slower"]
    slowest = data[data.threads.isin(categories)]["slowest"]

    print(fast)
    print(slower)
    print(slowest)
    
    plot_data = [
        go.Bar(name='Fast', x=categories, y=fast, marker_color="green"),
        go.Bar(name='Slower', x=categories, y=slower, marker_color="rgb(255,202,75)"),
        go.Bar(name='Slowest', x=categories, y=slowest, marker_color="crimson")
    ]
    fig = go.Figure(data=plot_data, layout=layout_)

    if not save:
        fig.show()
    else:
        save_dir = os.path.join(save_dir, "paths")
        os.makedirs(save_dir, exist_ok=True)
        filename = ("paths_" + str(ins_rate) + ".html")
        fig.write_html(os.path.join(save_dir, filename))


def get_threads_config():
    nthreads = []
    if FLAGS.detect_threads:
        print(
            'Automatically deriving thread configuration from "./config.mk"...'
        )
        nthreads.append(1)
        threads_config = parse_config("./config.mk")
        for i in range(
                threads_config["threadincrement"],
                threads_config["maxthreads"],
                threads_config["threadincrement"],
        ):
            nthreads.append(i)
        nthreads.append(threads_config["maxthreads"])
    else:
        assert FLAGS.nthreads is not None
        for n in FLAGS.nthreads:
            nthreads.append(int(n))
    return nthreads


def get_microbench_configs():
    if FLAGS.detect_experiments:
        print("Automatically detecting microbenchmark configurations")
        return parse_experiment_list_generate(
            FLAGS.generate_script,
            ["run_workloads", "run_rq_sizes"],
        )
    else:
        experiments = FLAGS.experiments
        experiment_configs = {}
        experiment_configs["datastructures"] = FLAGS.datastructures
        experiment_configs["ksizes"] = [
            int(max_key) for max_key in FLAGS.max_keys
        ]
        return experiments, experiment_configs


def main(argv):
    # If autodetect flag is set, then detect all configs automatically
    if FLAGS.autodetect:
        FLAGS.detect_threads = True
        FLAGS.detect_experiments = True
        FLAGS.detect_trials = True

    # Plot microbench results.
    if FLAGS.microbench:
        assert FLAGS.microbench_dir is not None
        FLAGS.workloads_urates = [int(u) for u in FLAGS.workloads_urates]

        nthreads = get_threads_config()
        print("Thread configuration: " + str(nthreads))
        #experiments, microbench_configs = get_microbench_configs()
        sizes = [100000000]
        # if FLAGS.datastructures is not None:
        #     microbench_configs["datastructures"] = FLAGS.datastructures
        # if FLAGS.max_keys is not None:
        #     microbench_configs["ksizes"] = [int(k) for k in FLAGS.max_keys]
        #print("Data structures and key ranges: " + str(microbench_configs))

        if FLAGS.detect_trials:
            runscript_config = parse_runscript(FLAGS.runscript, ["trials"])
            ntrials = runscript_config["trials"]
        else:
            ntrials = FLAGS.ntrials
        print("Number of trials: " + str(ntrials))
        #exp_type = "micro" # todo: make parameter

        # Plot peformance at different workload configurations (corresponds to Figure 2)
        for k in sizes:
            if FLAGS.exp_type == "micro":
                for u in FLAGS.workloads_urates: # iterate through workloads
                    plot_workload(
                        FLAGS.microbench_dir,
                        FLAGS.exp_type,
                        k,
                        u,
                        0,
                        nthreads,
                        ntrials,
                        FLAGS.yaxis_titles,
                        FLAGS.legends,
                        FLAGS.save_plots,
                        os.path.join(FLAGS.save_dir, "microbench"),
                    )
            if FLAGS.exp_type == "desg":
                for d in FLAGS.desg_tds:
                    plot_workload(
                        FLAGS.microbench_dir,
                        FLAGS.exp_type,
                        k,
                        0,
                        d,
                        nthreads,
                        ntrials,
                        FLAGS.yaxis_titles,
                        FLAGS.legends,
                        FLAGS.save_plots,
                        os.path.join(FLAGS.save_dir, "microbench"),
                    )
            if FLAGS.exp_type == "desg_sep":
                for d in FLAGS.desg_tds:
                #for d in [4]:
                    for type in ["ins", "del-min"]:
                        plot_desg_sep(
                            FLAGS.microbench_dir,
                            type,
                            k,
                            d,
                            nthreads,
                            ntrials,
                            FLAGS.yaxis_titles,
                            FLAGS.legends,
                            FLAGS.save_plots,
                            os.path.join(FLAGS.save_dir, "microbench"),
                        )
            
            if FLAGS.exp_type == "latency":
                for u in FLAGS.workloads_urates:
                    for type in ["ins", "del-min"]:
                        plot_lat_sep(
                            FLAGS.microbench_dir,
                            type,
                            k,
                            u,
                            nthreads,
                            ntrials,
                            FLAGS.yaxis_titles,
                            FLAGS.legends,
                            FLAGS.save_plots,
                            os.path.join(FLAGS.save_dir, "microbench"),
                        )

            if FLAGS.exp_type == "desg_usonly":
                for type in ["ins", "del-min"]:
                    plot_desg_usonly(
                        FLAGS.microbench_dir,
                        type,
                        k,
                        0,
                        nthreads,
                        ntrials,
                        FLAGS.yaxis_titles,
                        FLAGS.legends,
                        FLAGS.save_plots,
                        os.path.join(FLAGS.save_dir, "microbench"),
                    )

            if FLAGS.exp_type == "phased":
                #for phased_str in ["2,100:50000000,0:1000000", "2,100:50000000,0:5000000", "2,100:50000000,0:10000000"]:
                for phased_str in ["2,100:50000000,0:50000000", "2,100:50000000,0:25000000"]:
                    plot_workload(
                        FLAGS.microbench_dir,
                        FLAGS.exp_type,
                        k,
                        phased_str,
                        0,
                        nthreads,
                        ntrials,
                        FLAGS.yaxis_titles,
                        FLAGS.legends,
                        FLAGS.save_plots,
                        os.path.join(FLAGS.save_dir, "microbench"),
                    )
            if FLAGS.exp_type == "sssp":
                for graph in ["livejournal.txt", "roadnetCA.txt"]:
                #for graph in ["com-orkut.ungraph.txt"]:
                    plot_sssp(
                        FLAGS.sssp_dir, # sssp/data
                        FLAGS.exp_type, # sssp
                        graph,
                        nthreads,
                        ntrials,
                        FLAGS.yaxis_titles,
                        FLAGS.legends,
                        FLAGS.save_plots,
                        os.path.join(FLAGS.save_dir, "application"),
                    )

            if FLAGS.exp_type == "paths":
                for u in [50, 95]:
                    plot_paths(
                        FLAGS.microbench_dir,
                        FLAGS.exp_type,
                        k,
                        u,
                        nthreads,
                        ntrials,
                        FLAGS.yaxis_titles,
                        FLAGS.legends,
                        FLAGS.save_plots,
                        os.path.join(FLAGS.save_dir, "microbench"),
                    )



                
                # if "run_rq_sizes" in experiments:
                #     plot_rq_sizes(
                #         FLAGS.microbench_dir,
                #         ds,
                #         k,
                #         ntrials,
                #         FLAGS.rqsizes_rqsizes,
                #         FLAGS.yaxis_titles,
                #         FLAGS.legends,
                #         FLAGS.save_plots,
                #         os.path.join(FLAGS.save_dir, "microbench"),
                        
                #     )

    # Plot macrobench results (corresponds to Figure 4)
    if FLAGS.macrobench:
        save_dir = os.path.join(FLAGS.save_dir, "macrobench/skiplistlock")
        os.makedirs(save_dir, exist_ok=True)
        plot_macrobench(
            os.path.join(FLAGS.macrobench_dir, "rq_tpcc"),
            "SKIPLISTLOCK",
            ylabel=FLAGS.yaxis_titles,
            legend=FLAGS.legends,
            save=FLAGS.save_plots,
            save_dir=save_dir,
        )
        save_dir = os.path.join(FLAGS.save_dir, "macrobench/citrus")
        os.makedirs(save_dir, exist_ok=True)
        plot_macrobench(
            os.path.join(FLAGS.macrobench_dir, "rq_tpcc"),
            "CITRUS",
            ylabel=FLAGS.yaxis_titles,
            legend=FLAGS.legends,
            save=FLAGS.save_plots,
            save_dir=save_dir,
        )


if __name__ == "__main__":
    app.run(main)