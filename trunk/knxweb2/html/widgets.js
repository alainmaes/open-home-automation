/* for future use

*   $( "#overlay" ).click(function(e) {
    var coords = {
        clientX: e.clientX,
        clientY: e.clientY
    };
    $newdiv1.offset({ top: e.clientY - ($newdiv1.height() / 2), left: e.clientX - ($newdiv1.width() / 2)})
    // this actually triggers the drag start event
    $newdiv1.simulate("mousedown", coords);
  });
*/

function messageBox(message,title,icon) {
  // icon : alert, info, notice, help, mail-open, mail-closed, comment, person, trash, locked, unlocked, home, star, link, cancel, newwin, refresh
  // voir pour ajouter la class ui-state-highlight ou ui-state-error en fonction si "bon ou mauvais" 
	a=$('<div id="dialog-message" title="' + title + '">');
	if (icon != '') {
		a.html('<p><span class="ui-icon ui-icon-' + icon + '" style="float:left; margin:0 7px 50px 0;"></span>' + message + '</p>');
	} else {
    a.html('<p></span>' + message + '</p>');
  }
	a.dialog({
			width: 350,
			autosize: true,
			resizable: false,
			autoopen: true,
			modal: true,
			buttons: {
				Ok: function() {
					$( this ).dialog( "close" );
				}
			},
			close: function(event, ui) {
				$(this).dialog("destroy");
  			$(this).remove();
  	 	}
	});

}

function loadZone(name)
{
    $('.zone').each(function()
    {
        if (this.id == name)
            $(this).show();
        else
            $(this).hide();
    });
}

function getImageUrl(image)
{
    if ((image!=null) && (image!=""))
    {
        if (image.match(/^http:\/\//))
            return image;
        else
            return "pictures/" + image;
    }
    else
    {
        return "";
    }
}

function save()
{
    var xml = "<config width='" + $("#design").width() + 
              "' height='" + $("#design").height() +
              "' enableSlider='false'>\n <zones>\n";

    $("#design").children().each(function()
    {
        var background = jQuery(this).css('background-image');
        if (background)
        {
            var index = location.href.lastIndexOf("/") + 1;
            var url = location.href.substr(0, index) + "pictures/";
            background = background.replace('url("' + url, "");
            background = background.replace('")', "");
        }

        xml = xml + "  <zone id='" + this.id +
                    "' img='" + background + "' globalcontrol='false'>\n";

        $(this).children().each(function()
        {
            if (this.owner)
            {
                xml = xml + "   <control type='" + this.owner.type + "'";
                for(var key in this.owner.properties)
                {
                    xml = xml + " " + key + "='" +
                          this.owner.properties[key].value + "'";
                }
                xml = xml + ">\n   </control>\n";
            }
        });

        xml = xml + "  </zone>\n";
    });
    xml = xml + " </zones>\n";
    xml = xml + "</config>\n";

    var currentDesign = "testDesign";
    var version = "default";

    var url = 'design_technique.php?action=savedesign&name=' + currentDesign + '&ver=' + version;
			req = jQuery.ajax({ type: 'post', url: url, data: xml, processData: false, dataType: 'xml' ,
				success: function(responseXML, status) {
					var xmlResponse = responseXML.documentElement;
					if (xmlResponse.getAttribute('status') == 'success') {
						messageBox("Design saved successfully", "Info", "check");
					}
					else {
						messageBox("Error while saving design: "+xmlResponse.textContent, "Error", "alert");
					}
				},
				error: function (XMLHttpRequest, textStatus, errorThrown) {
					messageBox("Error while saving design: "+textStatus, "Error", "alert");
				}
			});
}

var WidgetFactory =
{
    widgetTypesList: new Array(),
    widgetList: new Array(),
    feedbackList: new Array(),

    registerWidget: function(widgetClass)
    {
        if (!widgetClass.prototype.type)
            alert("No type specified for widget!");
        else
            this.widgetTypesList[widgetClass.prototype.type] = widgetClass;
    },
    createWidget: function(widgetName, parent, conf)
    {
        if (!(widgetName in this.widgetTypesList))
        {
            //alert(widgetName + " is not a known widget");
            return undefined;
        }

        var obj = new this.widgetTypesList[widgetName](parent, conf);
        if (obj == undefined)
        {
            alert("widget creation failed");
            return undefined;
        }

        this.widgetList.push(obj);
        for(var key in obj.properties)
        {
            if (obj.properties[key].type == "object" &&
                obj.getProperty(key) != "" &&
                obj.getProperty(key) != undefined)
            {
                if (!(obj.getProperty(key) in this.feedbackList))
                {
                    this.feedbackList.push(obj.getProperty(key));
                }
            }
        }

        //if (obj.getProperty("feedback-object"))
        //{
        //    if (!(obj.getProperty("feedback-object") in this.feedbackList))
        //        this.feedbackList.push(obj.getProperty("feedback-object"));
        //}

        return obj;
    },

    updateWidgets: function(feedbackObj, value)
    {
        jQuery.each(this.widgetList, function(index, item) {
            item.update(feedbackObj, value);
        });
    },

    refreshValues: function(completeCallBack)
    {
        if (this.widgetList.length > 0)
        {
            var body = '<read><objects>';
            for (i=0; i < this.feedbackList.length; i++)
            {
                body += "<object id='" + this.feedbackList[i] + "'/>";
            }
	    body += "</objects></read>";

            var req = jQuery.ajax(
            {
                type: 'post',
                url: 'linknx.php?action=cmd',
                data: body,
                processData: false,
                dataType: 'xml',
                success: function(responseXML, status)
                {
                    var xmlResponse = responseXML.documentElement;
                    if (xmlResponse.getAttribute('status') != 'error')
                    {
                        // Send update to subscribers
                        var objs = xmlResponse.getElementsByTagName('object');
                        if (objs.length > 0)
                        {
                            for (i=0; i < objs.length; i++)
                            {
                                var element = objs[i];
                                WidgetFactory.updateWidgets(element.getAttribute('id'),element.getAttribute('value'));
                            }
                        }
                    }
                },
                error: function (XMLHttpRequest, textStatus, errorThrown) {
                    //UIController.setNotification(tr("Error: ")+textStatus);
                },
                complete: completeCallBack
            });
        }
        else if (completeCallBack)
            completeCallBack();
    },

    startUpdate: function()
    {
        WidgetFactory.refreshValues(function(XMLHttpRequest, textStatus) {
            setTimeout('WidgetFactory.startUpdate()', 1000);
        });
    },

    write: function(obj, value, successCallBack)
    {
        if (!obj)
            return;
        var body = "<write><object id='"+obj+"' value='"+value+"'/></write>";

        req = jQuery.ajax(
        {
            type: 'post',
            url: 'linknx.php?action=cmd',
            data: body,
            processData: false,
            dataType: 'xml' ,
            success: function(responseXML, status)
            {
                var xmlResponse = responseXML.documentElement;
                if (xmlResponse.getAttribute('status') == 'success')
                    WidgetFactory.updateWidgets(obj, value);
                else
                    alert("Error: "+xmlResponse.textContent);
                if (successCallBack)
                    successCallBack(response);
            }
        });
    },

    setEditMode: function(enabled)
    {
        if (enabled)
        {
            $("#widgets").show();
            jQuery.each(this.widgetList, function(index, item) {
                item.startEdit();
            });
        }
        else
        {
            $("#widgets").hide();
            $("#widgetProperties").html("");
            $("#widgetProperties").hide();
            jQuery.each(this.widgetList, function(index, item) {
                item.endEdit();
            });
            save();
        }
    },

    setSelected: function(object)
    {
        jQuery.each(this.widgetList, function(index, item) {
            item.div.css("border", "1px solid black");
        });
        object.div.css("border", "1px solid red");
    }
}

function cWidget()
{
    this.isEditMode = false;
    this.isResizable = true;
    this.div = undefined;
    this.properties = new Array();
    this.parent = undefined;
    this.activeActions = new Array();
    this.inactiveActions = new Array();

    /* Add properties common to all widgets */
    this.addProperty("name", "Name", "text", "");
    this.addProperty("x", "X", "text", 0);
    this.addProperty("y", "Y", "text", 0);
    this.addProperty("width", "Width", "text", 0);
    this.addProperty("height", "Height", "text", 0);
}

cWidget.prototype =
{
    type: undefined,

    init: function(parent, conf)
    {
        if (parent == undefined)
        {
            alert("init with undefined parent!");
            return;
        }
        this.parent = parent;

        this.div = $( "<div></div>" );
        this.div.get(0).owner = this;
        this.div.css('position', 'absolute');
        this.parent.append(this.div);

        this.div.click(function(e)
        {
            if (this.owner.isEditMode)
            {
                var table = $("<table />");
                $("#widgetProperties").html("");
                $("#widgetProperties").append(table);
                for(var key in this.owner.properties)
                {
                    var row = $("<tr>");
                    table.append(row);
                    var cell = $("<td>")
                    cell.html(this.owner.properties[key].display);
                    row.append(cell);
                    cell = $("<td>");
                    var input = $("<input type='text' class='prop' name='" + key + "' value='" +
                                  this.owner.properties[key].value + "' />");
                    input.get(0).owner = this.owner;
                    input.change(function() {
                        this.owner.setProperty(this.name, this.value);
                    });
                    input.on('input', function() {
                        this.owner.setProperty(this.name, this.value);
                    });
                    cell.append(input);
                    row.append(cell);
                }
                $("#widgetProperties").draggable();
                $("#widgetProperties").show();
                
                WidgetFactory.setSelected(this.owner);
            }
        });

        this.div.draggable({
            containment: this.parent,
            stop: function( event, ui )
            {
                this.owner.properties["x"].value = ui.position.left;
                this.owner.onSetProperty("x", ui.position.left);
                this.owner.properties["y"].value = ui.position.top;
                this.owner.onSetProperty("y", ui.position.top);
            }
        });
        this.div.draggable("disable");

        this.div.resizable({
            start: function(e, ui) {
                //alert('resizing started '+$(this).attr("id")+' w='+$(this).width());
            },
            resize: function(e, ui) {
                this.owner.onResize($(this).width(), $(this).height());
            },
            stop: function(e, ui) {
                this.owner.properties["width"].value = ui.size.width;
                this.owner.onSetProperty("width", ui.size.width);
                this.owner.properties["height"].value = ui.size.height;
                this.owner.onSetProperty("height", ui.size.height);

            }
        });
        this.div.resizable("disable");

        /* load the properties from XML */
        var widget = this;
        $(conf.attributes).each(function(index, attr)
        {
            if (attr.name in widget.properties) 
            {
                //alert(attr.name + " - " + attr.value);
                widget.setProperty(attr.name, attr.value);
            }
        });

        /* set all properties again after init is done */
        for(var key in this.properties)
        {
            this.setProperty(key, this.properties[key].value);
        }
    },
    addProperty: function(name, display, type, defVal)
    {
        var prop = {
            "display" : display,
            "type"    : type,
            "value"   : defVal
        }

        this.properties[name] = prop;
    },
    listProperties: function()
    {
        var keys = [];

        for(var key in this.properties)
        {
            keys.push(key);
        }

        alert(keys);
    },
    getProperty: function(name)
    {
        if (!(name in this.properties)) 
            return undefined;
        return this.properties[name].value;
    },
    setProperty: function(name, val)
    {
        this.properties[name].value = val;
        if (name == "x")
        {
            if (this.div)
                this.div.css("left", val + "px");
        }
        else if (name == "y")
        {
            if (this.div)
                this.div.css("top", val + "px");
        }
        else if (name == "width")
        {
            if (this.div)
            {
                this.div.css("width", val + "px");
                if (this.properties["height"] != undefined)
                    this.onResize(this.properties["width"].value, this.properties["height"].value);
            }
        }
        else if (name == "height")
        {
            if (this.div)
            {
                this.div.css("height", val + "px");
                if (this.properties["width"] != undefined)
                    this.onResize(this.properties["width"].value, this.properties["height"].value);
            }
        }
        this.onSetProperty(name, val);
    },
    startEdit: function()
    {
        this.isEditMode = true;
        this.div.draggable("enable");
        if (this.isResizable)
            this.div.resizable("enable");
        this.div.css("border", "1px solid black");
    },
    endEdit: function()
    {
        this.isEditMode = false;
        this.div.draggable("disable");
        if (this.isResizable)
            this.div.resizable("disable");
        this.div.css("border", "none");
    },    
    update: function(feedbackObj, value)
    {
        this.onUpdate(feedbackObj, value);
    },
    onMove: function(x,y)
    {
        /* to be implemented by derived class */
    },
    onResize: function(width, height)
    {
        /* to be implemented by derived class */
    },
    onSetProperty: function(name, val)
    {
        /* to be implemented by derived class */
    },
    onRefresh: function()
    {
        /* to be implemented by derived class */
    },
    onUpdate: function(feedbackObj, value)
    {
        /* to be implemented by derived class */
    },
    toggleValue: function(obj)
    {
        var body = "<execute><action type='toggle-value' id='"+obj+"'/></execute>";

        req = jQuery.ajax(
        {
            type: 'post',
            url: 'linknx.php?action=cmd',
            data: body,
            processData: false,
            dataType: 'xml' ,
            success: function(responseXML, status)
            {
                var xmlResponse = responseXML.documentElement;
                if (xmlResponse.getAttribute('status') != 'success')
                    alert("Error: "+xmlResponse.textContent);
            }
        });
    },
}

function cWidgetButton(parent, conf)
{
  cWidget.call(this);
    
                   /* name         display text         type       defVal */
  this.addProperty("picture",      "Picture",           "picture", "");
  this.addProperty("picture-active",   "Active picture",    "picture", "");
  this.addProperty("text",         "Text",              "text",    "Button Text");
  this.addProperty("size",     "Font size",         "text",    "12");
  this.addProperty("color",    "Color",             "color",   "#000000");
  this.addProperty("align",    "Alignment",         "text",    "");
  this.addProperty("text-padding",   "Top padding",       "text",    "");
  this.addProperty("leftPadding",  "Left padding",      "text",    "");
  this.addProperty("feedback-object",  "Feedback object",   "object",  "");
  this.addProperty("feedback-compare",  "Feedback compare",  "text",    "");
  this.addProperty("feedback-value",  "Feedback value",    "text",    "");
  this.addProperty("active-goto",   "Active goto",       "text",    "");
  this.addProperty("inactive-goto", "Inactive goto",     "text",    "");

  this.buttonDiv = $("<div id='buttonDiv'/>");

  this.init(parent, conf);

  this.div.append(this.buttonDiv);

  /* set some CSS attributes */
  //this.div.css('cursor', 'default');
  this.buttonDiv.css('background-repeat', 'no-repeat');

    this.div.click(function(e)
    {
        if (!this.owner.isEditMode)
        {
            if (this.owner.active)
            {
                if (this.owner.getProperty("feedback-object"))
                    this.owner.toggleValue(this.owner.getProperty("feedback-object"));
                    //WidgetFactory.write(this.owner.getProperty("feedback-object"), "off");
                if (this.owner.getProperty("active-goto"))
                    loadZone(this.owner.getProperty("active-goto"));
            }
            else
            {
                if (this.owner.getProperty("feedback-object"))
                    this.owner.toggleValue(this.owner.getProperty("feedback-object"));
                    //WidgetFactory.write(this.owner.getProperty("feedback-object"), "on");
                if (this.owner.getProperty("inactive-goto"))
                    loadZone(this.owner.getProperty("inactive-goto"));
            }
        }
    });
}

cWidgetButton.prototype = new cWidget();
cWidgetButton.prototype.type = "button";
WidgetFactory.registerWidget(cWidgetButton);

cWidgetButton.prototype.onResize = function(width, height)
{
    this.buttonDiv.css("width", width + "px");
    this.buttonDiv.css("height", height + "px");
}

cWidgetButton.prototype.onSetProperty = function(name, val)
{
  if (name == "text")
  {
    $(this.buttonDiv).html(val);
  }
  else if (name == "picture")
  {
    this.buttonDiv.css('background-image', 'url(' + getImageUrl(val) + ')');
    this.buttonDiv.css('background-size', '100%');
  }
  else if (name == "size")
  {
    this.buttonDiv.css('font-size', val + "px");
  }
  else if (name == "color")
  {
    this.buttonDiv.css('color', val);
  }
  else if (name == "align")
  {
    this.buttonDiv.css('text-align', val);
  }
  else if (name == "text-padding")
  {
    if (val != "")
      this.buttonDiv.css('padding-top', val + "px");
    else
      this.buttonDiv.css('padding-top', val);
  }
  else if (name == "leftPadding")
  {
    if (val != "")
      this.buttonDiv.css('padding-left', val + "px");
    else
      this.buttonDiv.css('padding-left', val);
  }
}

cWidgetButton.prototype.onUpdate = function(feedbackObj, value)
{
    if (feedbackObj == this.getProperty("feedback-object"))
    {
        var val = value;
	if (parseFloat(val))
            val = parseFloat(value);
	var feedback_val = this.getProperty("feedback-value");
	if (parseFloat(feedback_val))
            feedback_val = parseFloat(this.getProperty("feedback-value"));

        switch (this.getProperty("feedback-compare"))
        {
            case 'eq':
                this.active=(val==feedback_val);
                break;
            case 'neq':
                this.active=(val!=feedback_val);
                break;
            case 'gt':
                this.active=(val>feedback_val);
                break;
            case 'lt':
                this.active=(val<feedback_val);
                break;
            case 'gte':
                this.active=(val>=feedback_val);
                break;
            case 'lte':
                this.active=(val<=feedback_val);
                break;
            default:
                this.active=false;
        }

        var picture=((this.active) ? this.getProperty("picture-active") : this.getProperty("picture"));
	if (picture!="") this.buttonDiv.css('background-image', 'url(' + getImageUrl(picture) + ')');
    }
}

/******************************************************************************/

function cWidgetGauge(parent, conf)
{
  cWidget.call(this);

                 /* name          display text         type       defVal */
  this.addProperty("label",       "Title",             "text",    "");
  this.addProperty("feedback-object", "Feedback object",   "object",  "");
  this.addProperty("min",         "Min temp",   "text",  "0");
  this.addProperty("max",         "Max temp",   "text",  "50");

  this.init(parent, conf);

  this.googDiv = $( "<div></div>" );
  var a = this.googDiv.get(0);
  a.owner = this;
  this.div.append(this.googDiv);

  this.data = google.visualization.arrayToDataTable([
    ['Label', 'Value'],
    [this.getProperty("label"), 0]
  ]);

  this.options = {
    width: this.getProperty("width"), height: this.getProperty("height"),
    min: parseInt(this.getProperty("min")), max: parseInt(this.getProperty("max"))
  };

  this.chart = new google.visualization.Gauge(this.googDiv[0]);
  this.chart.draw(this.data, this.options);
}

cWidgetGauge.prototype = new cWidget();
cWidgetGauge.prototype.type = "gauge";
WidgetFactory.registerWidget(cWidgetGauge);

cWidgetGauge.prototype.onSetProperty = function(name, val)
{
    if (name == "label")
    {
        if (this.chart)
        {
            this.data.setValue(0, 0, val);
            this.chart.draw(this.data, this.options);
        }
    }
    else if (name == "min")
    {
        if (this.options && parseInt(val))
        {
            this.options.min = parseInt(val);
            if (this.chart)
                this.chart.draw(this.data, this.options);
        }
    }
    else if (name == "max")
    {
        if (this.options && parseInt(val))
        {
            this.options.max = parseInt(val);
            if (this.chart)
                this.chart.draw(this.data, this.options);
        }
    }
}

cWidgetGauge.prototype.onResize = function(width, height)
{
    if (this.chart)
    {
        this.options.width = width;
        this.options.height = height;
        this.chart.draw(this.data, this.options);
    }
}

cWidgetGauge.prototype.onRefresh = function()
{
    if (this.chart)
    {
        this.chart.draw(this.data, this.options);
    }
}

cWidgetGauge.prototype.onUpdate = function(feedbackObj, value)
{
    if (feedbackObj == this.getProperty("feedback-object"))
    {
        if (this.chart)
        {
            if (this.lastValue == value)
                return;

            var val = parseFloat(value);
            //val = val.toFixed(2);
            this.data.setValue(0, 1, val);
            this.chart.draw(this.data, this.options);
            this.lastValue = value;
        }
    }
}

/******************************************************************************/

function cWidgetWaterLevel(parent, conf)
{
  cWidget.call(this);

                 /* name          display text         type       defVal */
  this.addProperty("label",       "Title",             "text",    "");
  this.addProperty("feedback-object", "Feedback object",   "object",  "");
  this.addProperty("diameter",         "Diameter",   "text",  "30");
  this.addProperty("maxWater",         "Max water depth",   "text",  "200");
  this.addProperty("maxDepth",         "Max depth",   "text",  "30");

  this.init(parent, conf);

  this.googDiv = $( "<div></div>" );
  var a = this.googDiv.get(0);
  a.owner = this;
  this.div.append(this.googDiv);

  this.data = google.visualization.arrayToDataTable([
    ['Label', 'Value'],
    [this.getProperty("label"), 0]
  ]);

  var waterHeight = parseInt(this.getProperty("maxDepth")) -
                    parseInt(this.getProperty("maxWater"));
  var volume = parseInt(this.getProperty("diameter")) / 200.00;
  volume = Math.pow(volume, 2);
  volume *= Math.PI;
  volume *= (waterHeight / 100.00);
  volume = Math.round(volume * 1000);

  this.options = {
    width: this.getProperty("width"), height: this.getProperty("height"),
    min: 0, max: volume, redFrom: 0, redTo: (volume * 25) / 100,
    yellowFrom: (volume * 25) / 100, yellowTo: (volume * 50) / 100,
    greenFrom: (volume * 50) / 100, greenTo: volume,
    minorTicks: 1000, majorTicks: 500
  };

  this.chart = new google.visualization.Gauge(this.googDiv[0]);
  this.chart.draw(this.data, this.options);
}

cWidgetWaterLevel.prototype = new cWidget();
cWidgetWaterLevel.prototype.type = "waterlevel";
WidgetFactory.registerWidget(cWidgetWaterLevel);

cWidgetWaterLevel.prototype.onSetProperty = function(name, val)
{
  //
}

cWidgetWaterLevel.prototype.onResize = function(width, height)
{
    if (this.chart)
    {
        this.options.width = width;
        this.options.height = height;
        this.chart.draw(this.data, this.options);
    }
}

cWidgetWaterLevel.prototype.onRefresh = function()
{
    if (this.chart)
    {
        this.chart.draw(this.data, this.options);
    }
}

cWidgetWaterLevel.prototype.onUpdate = function(feedbackObj, value)
{
    if (feedbackObj == this.getProperty("feedback-object"))
    {
        if (this.chart)
        {
            if (this.lastValue == value)
                return;

            if (!(parseInt(value)))
                return;

            var waterHeight = parseInt(this.getProperty("maxDepth")) - parseInt(value);
            var volume = parseInt(this.getProperty("diameter")) / 200.00;
            volume = Math.pow(volume, 2);
            volume *= Math.PI;
            volume *= (waterHeight / 100.00);
            this.data.setValue(0, 1, Math.round(volume * 1000));
            
            this.chart.draw(this.data, this.options);
            this.lastValue = value;
        }
    }
}

/******************************************************************************/

function cWidgetAudio(parent, conf)
{
  cWidget.call(this);
  this.isResizable = false;

                 /* name          display text         type       defVal */
  this.addProperty("titleLabel",       "Title",             "text",    "Audio");
  this.addProperty("feedback-object", "Feedback object",   "object",  "");
  this.addProperty("tunerLabel", "Tuner",   "text",  "Radio");
  this.addProperty("aux1Label", "Aux1",   "text",  "Aux1");
  this.addProperty("aux2Label", "Aux2",   "text",  "Aux2");
  this.addProperty("aux3Label", "Aux3",   "text",  "Aux3");
  this.addProperty("aux4Label", "Aux4",   "text",  "Aux4");
  this.addProperty("min", "Minimum value",   "text",  "0");
  this.addProperty("max", "Maximum value",   "text",  "100");
  this.addProperty("border-color", "Border color",   "text",  "black");

  this.setProperty("width", 178); //193);
  this.setProperty("height", 130);

    /*<setting id="slider-color" label="Slider color" type="color" default="#FF0000" />
    <setting id="border" label="Border" type="list" default="true">
      <option key="true" value="Yes" />
      <option key="false" value="No" />
    </setting>
    <setting id="border-color" label="Border color" type="color" default="#000000" />
    <setting id="orientation" label="Orientation" type="list" default="horizontal">
      <option key="horizontal" value="horizontal"/>
      <option key="vertical" value="Vertical"/>
    </setting>
    <setting id="position" label="Position" type="list" default="right_bottom">
      <option key="left_top" value="left or top"/>
      <option key="right_bottom" value="right or bottom"/>
    </setting>
    </settings>*/

  this.init(parent, conf);

  this.div.css('font-family', "arial, verdana, sans-serif");
  this.div.css('font-size', "16px");
  this.div.css('overflow', "hidden");

  var container = $("<div />");
  container.css('border', "1px solid " + this.getProperty("border-color"));
  container.css('border-radius', "15px");
  
  container.append($("<table id='widgetTable'> \
                       <tr> \
                         <td> \
                           <table> \
                             <tr> \
                               <th><div id='title' /></th> \
                               <td width='32px' height='32px'><div id='state' /></td> \
                             </tr> \
                           </table> \
                         </td> \
                       </tr> \
                       <tr> \
                         <td> \
                           <table> \
                             <tr> \
                               <td width='20%'>Input:</td> \
                               <td> \
                                 <select id='inputSelection' /> \
                               </td> \
                             </tr> \
                             <tr> \
                               <td width='20%'>Channel:</td> \
                               <td> \
                                 <select id='channelSelection' /> \
                               </td> \
                             </tr> \
                           </table> \
                         </td> \
                       </tr> \
                       <tr> \
                         <td><div id='slider'></div></td> \
                       </tr> \
                     </table>"));

    this.div.append(container);
    $("#widgetTable", this.div).css('width', this.div.width() + 'px');
    $("#widgetTable", this.div).css('height', (this.div.height() - 2) + 'px');
    
    /* set up title */
    if (this.getProperty("titleLabel") != "")
        $("#title", this.div).text(this.getProperty("titleLabel"));
    else
        $("#title", this.div).text(this.getProperty("feedback-object"));
    $("#title", this.div).css('width', (this.div.width() - 42) + 'px');
    $("#title", this.div).parent().css('width', (this.div.width() - 42) + 'px');

    /* set up the icon */
    $("#state", this.div).css('background-image', 'url(' + getImageUrl("32x32_sonOff.png") + ')');
    $("#state", this.div).css('width', '32px');
    $("#state", this.div).css('height', '32px');
    $("#state", this.div).css('left', (this.div.width() - 32) + 'px');
    $("#state", this.div).css('top', '0px');
    $('#state', this.div )[0].owner = this;
    $("#state", this.div).click(function() {
        if (!this.owner.editMode)
        {
            this.owner.sendVolumeToAmpli(0);
        }
    });

    /* set up the input selection box */
    $('#inputSelection', this.div).css('font-size', "11px");
    var inputs;
    if (this.getProperty("tunerLabel") != "")
        $('#inputSelection', this.div).append(
          '<option value="TUNE">' + this.getProperty("tunerLabel") + '</option>');
    else
        $('#inputSelection', this.div).append('<option value="TUNE">Tuner</option>');
    if (this.getProperty("aux1Label") != "")
        $('#inputSelection', this.div).append(
          '<option value="AUX1">' + this.getProperty("aux1Label") + '</option>');
    else
        $('#inputSelection', this.div).append(
          '<option value="AUX1">Aux1</option>');
    if (this.getProperty("aux2Label") != "")
        $('#inputSelection', this.div).append(
          '<option value="AUX2">' + this.getProperty("aux2Label") + '</option>');
    else
        $('#inputSelection', this.div).append(
          '<option value="AUX2">Aux2</option>');
    if (this.getProperty("aux3Label") != "")
        $('#inputSelection', this.div).append(
          '<option value="AUX3">' + this.getProperty("aux3Label") + '</option>');
    else
        $('#inputSelection', this.div).append(
          '<option value="AUX3">Aux3</option>');
    if (this.getProperty("aux4Label") != "")
        $('#inputSelection', this.div).append(
          '<option value="AUX4">' + this.getProperty("aux4Label") + '</option>');
    else
        $('#inputSelection', this.div).append(
          '<option value="AUX4">Aux4</option>');

    var inputSelection = $('#inputSelection', this.div);
    inputSelection[0].owner = this;
    inputSelection.bind("change", function()
    {
        this.owner.sendInputToAmpli($("#inputSelection", this.owner.div).val());
    });


  $('#channelSelection', this.div).css('font-size', "11px");
  $('#channelSelection', this.div).append(
            '<option value="89">MNM</option>');
  $('#channelSelection', this.div).append(
            '<option value="94.2">Radio 1</option>');
  $('#channelSelection', this.div).append(
            '<option value="97.5">Radio 2</option>');
  $('#channelSelection', this.div).append(
            '<option value="90.6">Joe</option>');
  $('#channelSelection', this.div).append(
            '<option value="92.9">Q</option>');
  $('#channelSelection', this.div).append(
            '<option value="98">Minerva</option>');
  $('#channelSelection', this.div).append(
            '<option value="100.2">FG DJ radio</option>');
  $('#channelSelection', this.div).append(
            '<option value="100.9">Studio Brussel</option>');
  $('#channelSelection', this.div).append(
            '<option value="102.9">Nostalgie</option>');
  $('#channelSelection', this.div).append(
            '<option value="96.4">Klara</option>');


  var channelSelection = $('#channelSelection', this.div);
  channelSelection[0].owner = this;
  channelSelection.bind("change", function()
  {
    val = parseFloat($("#channelSelection", this.owner.div).val());
    val *= 10;
    val = Math.round(val);
    this.owner.sendFrequencyToAmpli(val);
  });

  /* volume slider */ 
  if (this.getProperty("min") != "") this.min = parseInt(this.getProperty("min"));
  if (this.getProperty("max") != "") this.max = parseInt(this.getProperty("max"));
   if (this.min < 0)
      this.min = 0;
  if (this.max > 100)
      this.max = 100;

  this.position = this.getProperty("position");
  this.orientation = this.getProperty("orientation");
  var disabled = false; //TODO

  if (this.min > this.max) {
    var max = this.max;
    this.max = this.min;
    this.min = max;
  }
 
  this.oldvalue = (this.min - 1);
 
 
  var delta = (this.max - this.min); 
  this.delta = delta;

  var step = parseInt( delta / (parseInt(this.getProperty("width"))) );
  if (step < 1) step = 1;

  var range = 'min';
  
  var slider = $('#slider', this.div);
  slider.owner = this;

  //slider.slider( "destroy" );
  slider.slider({  
        orientation: this.getProperty("orientation"),
        range: range,
        min: 0,
        max: delta,
        value: parseInt( delta / 2), 
        step: step,
        disabled: disabled,
  
        start: function(event,ui) {  
        },  
  
        slide: function(event, ui) {  
  
            var value = slider.slider('value'),  
                volume = $('.volume');  
 
            if(value <= 5) {   
                volume.css('background-position', '0 0');  
            }   
            else if (value <= 25) {  
                volume.css('background-position', '0 -25px');  
            }   
            else if (value <= 75) {  
                volume.css('background-position', '0 -50px');  
            }   
            else {  
                volume.css('background-position', '0 -75px');  
            };  
  
        }
    }); 

    slider.css('width', (this.div.width() - 30) + 'px');
    slider.css('left', '15px');

    if (this.editMode)  {
        this.updateObject(this.conf.getAttribute("feedback-object"),(( this.delta / 2 ) + this.min ));
    } else {
        $( '#slider', this.div )[0].owner = this;
        slider.bind( "slidestop", function(event, ui)
	{
	    var value = this.owner.convertFromUiValue(ui.value);
	    $(".value",this.owner.div).text(value).show(); // + "ui:" + ui.value
	    this.owner.sendVolumeToAmpli(value);
            $(".value",this.owner.div).hide();
        });
        slider.bind( "slide", function(event, ui) {
            var value = this.owner.convertFromUiValue(ui.value);
	    this.owner.sendVolumeToAmpli(value);
            $(".value",this.owner.div).text(value).show();
        });
    }
}

cWidgetAudio.prototype = new cWidget();
cWidgetAudio.prototype.type = "audio";
WidgetFactory.registerWidget(cWidgetAudio);

cWidgetAudio.prototype.onSetProperty = function(name, val)
{
    //
}

cWidgetAudio.prototype.onResize = function(width, height)
{
    //
}

cWidgetAudio.prototype.onRefresh = function()
{
    //
}

cWidgetAudio.prototype.sendVolumeToAmpli = function(volume) {
    if (this.editMode)
        return;

    if (volume < 16)
        newValue = "0" + volume.toString(16) + this.oldvalue.substr(2);
    else
        newValue = volume.toString(16) + this.oldvalue.substr(2);
 
    if (!this.isEditMode && this.getProperty("feedback-object"))
        WidgetFactory.write(this.getProperty("feedback-object"), newValue);
}

cWidgetAudio.prototype.sendInputToAmpli = function(input) {
    if (this.editMode)
        return;

    newValue = this.oldvalue.substr(0, 3) + input + this.oldvalue.substr(7);
 
    if (!this.isEditMode && this.getProperty("feedback-object"))
        WidgetFactory.write(this.getProperty("feedback-object"), newValue);
}

cWidgetAudio.prototype.sendFrequencyToAmpli = function(frequency) {
    if (this.editMode)
        return;

    var value = ((frequency - (frequency % 10)) / 10).toString(16) + "-";
    var value2 = ((frequency % 10) * 1000).toString(16);
    var command = "";

    if (value2.length == 4)
        command = value + value2;
    else if (value2.length == 3)
        command = value + "0" + value2;
    else if (value2.length == 2)
        command = value + "00" + value2;
    else if (value2.length == 1)
        command = value + "000" + value2;
    else if (value2.length == 0)
        command = value + "0000";

    command = this.oldvalue.substr(0, 8) + command.toUpperCase();

    if (!this.isEditMode && this.getProperty("feedback-object"))
        WidgetFactory.write(this.getProperty("feedback-object"), command);
}

cWidgetAudio.prototype.convertFromUiValue = function(valueui) {
  var value = valueui + this.min;
  return value;  
}

cWidgetAudio.prototype.convertToUiValue = function(value) {
  if (value < this.min ) value = this.min;
  else if (value > this.max ) value = this.max;
  var valueui = parseInt(value, 16) + parseInt(this.min);
  return valueui;  
}

cWidgetAudio.prototype.onUpdate = function(feedbackObj, value)
{
    if (feedbackObj == this.getProperty("feedback-object"))
    {
	if (this.lastValue == value)
           return;
            
        if (value.length != 15 || value.charAt(2) != '-' ||
        value.charAt(7) != '-' || value.charAt(10) != '-')
        //alert("invalid value");
        return;

    if (this.oldvalue != value ) {
      this.oldvalue = value;
      
      this.volume = value.substr(0, 2);
      this.input = value.substr(3, 4);
      this.freq = parseInt(value.substr(8, 2), 16) + '.' +
                  parseInt(value.substr(11), 16);
      this.freq = Math.round(parseFloat(this.freq) * 10) / 10;

      /* update slider */
      var valueui = this.convertToUiValue(this.volume);
      var slider = $('#slider', this.div);
      slider.slider( "value" , valueui );
    
      if (valueui == 0)
      {
          $("#state", this.div).css('background-image', 'url(' + getImageUrl("32x32_sonOff.png") + ')');
      }
      else
      {
          $("#state", this.div).css('background-image', 'url(' + getImageUrl("32x32_sonOn.png") + ')');
      }

      /* update input select box */
      var select = $('#inputSelection', this.div);
      if(select.prop) {
        var options = select.prop('options');
      }
      else {
        var options = select.attr('options');
      }
      select.val(this.input);

      /* update channel select box */
      var channelSelect = $('#channelSelection', this.div);
      //alert(this.freq);
      channelSelect.val(this.freq);
      this.lastValue = parseInt(value);
    }
    }
}

/******************************************************************************/

function cWidgetKnob(parent, conf)
{
    cWidget.call(this);

                 /* name          display text         type       defVal */
    this.addProperty("feedback-object", "Feedback object",   "object",  "");
    this.addProperty("min", "Minimum value",   "text",  "0");
    this.addProperty("max", "Maximum value",   "text",  "100");

    this.init(parent, conf);

    var container = $("<input type='text' id='knob'>");
    this.div.append(container);
    container.click(function(e)
    {
        if (!this.owner.isEditMode)
        {
            if (this.owner.getProperty("feedback-object"))
                WidgetFactory.write(this.owner.getProperty("feedback-object"), "off");
        }
    });

    $('#knob', this.div )[0].owner = this;
    $("#knob", this.div).knob({
        width: parseInt(this.getProperty("width")),
        height: parseInt(this.getProperty("height")),
        min: parseInt(this.getProperty("min")),
        max: parseInt(this.getProperty("max")),
        owner: this,
        change : function (value) {
            console.log("change : " + value);
            WidgetFactory.write(this.owner.getProperty("feedback-object"), value);
        },
        release : function (value) {
            //console.log(this.$.attr('value'));
            console.log("release : " + value);
            WidgetFactory.write(this.owner.getProperty("feedback-object"), value);
        },
        cancel : function () {
            console.log("cancel : ", this);
        }
    });
    $('#knob', this.div ).knob.owner = this;
    

    //$("#knob", this.div).val(27).trigger('change');
    //$("#knob").knob({ width: 200, height:200});
    /*$("#knob", this.div).trigger('configure',
                                 {
                                     "min":10,
                                     "max":40
                                 });*/
}

cWidgetKnob.prototype = new cWidget();
cWidgetKnob.prototype.type = "knob";
WidgetFactory.registerWidget(cWidgetKnob);

cWidgetKnob.prototype.onSetProperty = function(name, val)
{
  //
}

cWidgetKnob.prototype.onResize = function(width, height)
{
    $("#knob", this.div).trigger('configure',
                                 {
                                     "width": width,
                                     "height" : height
                                 });
    $("#knob", this.div).trigger('resize');
}

cWidgetKnob.prototype.onRefresh = function()
{
    /*if (this.chart)
    {
        this.chart.draw(this.data, this.options);
    }*/
}

cWidgetKnob.prototype.onUpdate = function(feedbackObj, value)
{
    if (feedbackObj == this.getProperty("feedback-object"))
    {
        if (this.lastValue != value)
        {
            $("#knob", this.div).val(parseInt(value)).trigger('change');
            this.lastValue = value;
        }
    }
}

/******************************************************************************/
var thermoCounter = 0;

function cWidgetThermometer(parent, conf)
{
    cWidget.call(this);
    this.isResizable = false;

    this.instId = 'thermo-' + thermoCounter;
    thermoCounter++;
    
                 /* name          display text         type       defVal */
    this.addProperty("label",       "Title",             "text",    "");
    this.addProperty("feedback-object", "Feedback object",   "object",  "");
    this.addProperty("min", "Minimum value",   "text",  "0");
    this.addProperty("max", "Maximum value",   "text",  "100");

    this.init(parent, conf);

    this.setProperty("width", 250); //193);
    this.setProperty("height", 130);

    var container = $("<canvas id='" + this.instId + "' width='" + this.getProperty("width") + "' height='" + this.getProperty("height") + "'>");
    this.div.append(container);
    var text = $("<div id='label'></div>");
    text.css("text-align", "center");
    text.text(this.getProperty("label"));
    this.div.append(text);

    $('#' + this.instId, this.div )[0].owner = this;
    
    container.thermometer({
        w: parseInt(this.getProperty("width")),
        h: parseInt(this.getProperty("height")),
        color: {
            //label: 'rgba(255, 255, 255, 1)',
            tickLabel: 'rgba(255, 0, 0, 0.4)'
        },
        centerTicks: false,
        majorTicks: 2,
        minorTicks: 4,
        max: parseInt(this.getProperty("max")),
        min: parseInt(this.getProperty("min")),
        scaleTickLabelText: 1.15,
        scaleLabelText: 1,
        scaleTickWidth: 1,
        unitsLabel: "C",
        bulbRadius: 30
    });
}

cWidgetThermometer.prototype = new cWidget();
cWidgetThermometer.prototype.type = "thermometer";
cWidgetThermometer.prototype.counter = 0;
WidgetFactory.registerWidget(cWidgetThermometer);

cWidgetThermometer.prototype.onSetProperty = function(name, val)
{
  //
}

cWidgetThermometer.prototype.onResize = function(width, height)
{
    //
}

cWidgetThermometer.prototype.onRefresh = function()
{
    /*if (this.chart)
    {
        this.chart.draw(this.data, this.options);
    }*/
}

cWidgetThermometer.prototype.onUpdate = function(feedbackObj, value)
{
    if (feedbackObj == this.getProperty("feedback-object"))
    {
        if (this.lastValue != value)
        {
            $('#' + this.instId).thermometer('setValue', parseFloat(value));
            this.lastValue = value;
        }
    }
}

/******************************************************************************/

function cWidgetThermostat(parent, conf)
{
    cWidget.call(this);
    this.isResizable = false;

    
                 /* name          display text         type       defVal */
    this.addProperty("label",       "Title",             "text",    "");
    this.addProperty("min", "Minimum value",   "text",  "0");
    this.addProperty("max", "Maximum value",   "text",  "100");
    this.addProperty("border-color", "Border color",   "text",  "black");
    this.addProperty("feedback-object", "Thermometer object",   "object",  "");
    this.addProperty("feedback-object-min", "Min object",   "object",  "");
    this.addProperty("feedback-object-max", "Max object",   "object",  "");
    this.addProperty("feedback-correction", "Thermometer correction",   "text",  "0");
    this.addProperty("temperature-preset", "Preset object",   "object",  "");

    this.init(parent, conf);

    this.setProperty("width", 250); //193);
    this.setProperty("height", 160);

    var container = $("<div />");
    container.css("width", this.getProperty("width") + "px");
    container.css("height", this.getProperty("height") + "px");
    container.css('font-family', "arial, verdana, sans-serif");
    container.css('font-size', "16px");
    container.css('overflow', "hidden");
    container.css('border', "1px solid " + this.getProperty("border-color"));
    container.css('border-radius', "15px");
    container.css('background-color', "rgba(255, 255, 255, 0.7)");
    this.div.append(container);

    var table =
        $("<table width='100%'> \
            <tr> \
             <td> \
               <table width='100%'> \
                <tr> \
                 <td id='state'></td> \
                 <td>"+this.getProperty("label")+"</td> \
                </tr> \
               </table> \
             </td> \
            </tr> \
            <tr> \
             <td> \
               <table width='100%'> \
                <tr> \
                 <td><input type='text' id='knob-temp'></td> \
                 <td id='info' > \
                  <table width='100%'> \
                   <tr> \
                    <td>Huidig:</td> \
                    <td><div id='current' /></td> \
                   </tr> \
                   <tr> \
                    <td>Mode:</td> \
                    <td>Nacht</td> \
                   </tr> \
                   <tr> \
                    <td>Min:</td> \
                    <td><div id='min' /></td> \
                   </tr> \
                   <tr> \
                    <td>Max:</td> \
                    <td><div id='max' /></td> \
                   </tr> \
               </table> \
                </tr> \
               </table> \
             </td> \
            </tr> \
           </table>");
    container.append(table);

    /* set up the icon */
    $("#state", this.div).css('background-image', 'url(' + getImageUrl("thermometer.png") + ')');
    $("#state", this.div).css('width', '32px');
    $("#state", this.div).css('height', '32px');
    
    $("#info", this.div).css('border', "1px solid " + this.getProperty("border-color"));
    $("#info", this.div).css('border-radius', "15px");
    $("#info", this.div).css('width', '120px');
    $("#info", this.div).css('height', '100px');

    $('#knob-temp', this.div).click(function(e)
    {
        if (!this.owner.isEditMode)
        {
            //alert("Click");
            //if (this.owner.getProperty("feedback-object"))
            //    WidgetFactory.write(this.owner.getProperty("feedback-object"), "off");
        }
    });

    $('#knob-temp', this.div )[0].owner = this;
    $("#knob-temp", this.div).knob({
        width: 100,
        height: 100,
        fgColor: "rgb(150, 150, 255)",
        bgColor: "rgb(255, 255, 255)",
        min: parseInt(this.getProperty("min")),
        max: parseInt(this.getProperty("max")),
        owner: this,
        change : function (value) {
            //console.log("change : " + value);
            WidgetFactory.write(this.owner.getProperty("temperature-preset"), value);
        },
        release : function (value) {
            //console.log(this.$.attr('value'));
            //console.log("release : " + value);
            WidgetFactory.write(this.owner.getProperty("temperature-preset"), value);
        },
        cancel : function () {
            console.log("cancel : ", this);
        }
    });
    $('#knob-temp', this.div ).knob.owner = this;
}

cWidgetThermostat.prototype = new cWidget();
cWidgetThermostat.prototype.type = "thermostat";
cWidgetThermostat.prototype.counter = 0;
WidgetFactory.registerWidget(cWidgetThermostat);

cWidgetThermostat.prototype.onSetProperty = function(name, val)
{
  //
}

cWidgetThermostat.prototype.onResize = function(width, height)
{
    //
}

cWidgetThermostat.prototype.onRefresh = function()
{
    /*if (this.chart)
    {
        this.chart.draw(this.data, this.options);
    }*/
}

cWidgetThermostat.prototype.onUpdate = function(feedbackObj, value)
{
    if (feedbackObj == this.getProperty("feedback-object"))
    {
        if (this.lastValue != value)
        {
            var newVal = parseFloat(value);
            if (parseFloat(this.getProperty("feedback-correction")))
                newVal += parseFloat(this.getProperty("feedback-correction"));
            $('#current', this.div).text(newVal.toFixed(1));
            this.lastValue = newVal;
        }
    }
    else if (feedbackObj == this.getProperty("feedback-object-min"))
    {
        if (this.lastMinValue != value)
        {
            var newVal = parseFloat(value);
            $('#min', this.div).text(newVal.toFixed(1));
            this.lastMinValue = newVal;
        }
    }
    else if (feedbackObj == this.getProperty("feedback-object-max"))
    {
        if (this.lastMaxValue != value)
        {
            var newVal = parseFloat(value);
            $('#max', this.div).text(newVal.toFixed(1));
            this.lastMaxValue = newVal;
        }
    }
    else if (feedbackObj == this.getProperty("temperature-preset"))
    {
        if (this.lastPresetValue != value)
        {
            $("#knob-temp", this.div).val(parseInt(value)).trigger('change');
            this.lastPresetValue = value;
        }
    }
}

/******************************************************************************/

function cWidgetSMS(parent, conf)
{
  cWidget.call(this);
    
                   /* name     display text         type       defVal */
  this.addProperty("size",     "Font size",         "text",    "12");
  this.addProperty("color",    "Color",             "color",   "#000000");

  this.smsDiv = $("<div id='smsDiv' />");

  this.init(parent, conf);

  /* set some CSS attributes */
  //this.div.css('cursor', 'default');
  //this.smsDiv.css('background-repeat', 'no-repeat');
  this.smsDiv.css("width", this.getProperty("width") + "px");
  this.smsDiv.css("height", this.getProperty("height") + "px");
  this.smsDiv.css('font-family', "arial, verdana, sans-serif");
  this.smsDiv.css('font-size', "16px");
  this.smsDiv.css('overflow', "hidden");
  this.smsDiv.css('border', "1px solid black");
  this.smsDiv.css('border-radius', "15px");
  this.smsDiv.css('background-color', "rgba(255, 255, 255, 0.7)");
  this.smsDiv.css('overflow-y', "auto");
  this.div.append(this.smsDiv);

  var audioContainer = $('<audio><source src="Iphone_Original_Tune.ogg" type="audio/ogg"><source src="Iphone_Original_Tune.mp3" type="audio/mpeg"></audio>');

  this.div.append(audioContainer);

  this.div.click(function(e)
  {
    if (!this.owner.isEditMode)
    {
            if (this.owner.active)
            {
                /*if (this.owner.getProperty("feedback-object"))
                    this.owner.toggleValue(this.owner.getProperty("feedback-object"));
                    //WidgetFactory.write(this.owner.getProperty("feedback-object"), "off");
                if (this.owner.getProperty("active-goto"))
                    loadZone(this.owner.getProperty("active-goto"));*/
            }
            else
            {
                /*if (this.owner.getProperty("feedback-object"))
                    this.owner.toggleValue(this.owner.getProperty("feedback-object"));
                    //WidgetFactory.write(this.owner.getProperty("feedback-object"), "on");
                if (this.owner.getProperty("inactive-goto"))
                    loadZone(this.owner.getProperty("inactive-goto"));*/
            }
    }
  });

  this.retrieveMessages();
  this.poll();
}

cWidgetSMS.prototype = new cWidget();
cWidgetSMS.prototype.type = "sms";
WidgetFactory.registerWidget(cWidgetSMS);

cWidgetSMS.prototype.onResize = function(width, height)
{
    this.smsDiv.css("width", width + "px");
    this.smsDiv.css("height", height + "px");
}

cWidgetSMS.prototype.onSetProperty = function(name, val)
{
  if (name == "size")
  {
    this.smsDiv.css('font-size', val + "px");
  }
  else if (name == "color")
  {
    this.smsDiv.css('color', val);
  }
}

cWidgetSMS.prototype.refreshHtml = function(xmlResponse)
{
    if (xmlResponse.getAttribute('status') != 'error')
    {
        var oldHtml = this.smsDiv.html();

        var htmlString = "<div style=\"padding-left:5px; font-size:18px; color:black;\"><img src=\"images/message-32.png\" width=32 height=32 /> Messages</div>";
        htmlString    += "<table width=\"100%\">";
        htmlString    += "<tr>";
        htmlString    += "<th width='150px' align='left'>From</th>";
        htmlString    += "<th align='left'>Text</th>";
        htmlString    += "<th width='20px' align='left'><img id=\"sendSms\" src=\"images/new.png\" width=20 height=20 /></th>";
        htmlString    += "</tr>";

        // Find all the sms messages
        var objs = xmlResponse.getElementsByTagName('message');
        if (objs.length > 0)
        {
            for (i=0; i < objs.length; i++)
            {
                var element = objs[i];
                htmlString += "<tr><td>";
                htmlString += element.getAttribute('from');
                htmlString += "</td><td>";
                htmlString += element.getAttribute('message');
                htmlString += "</td><td>";
                htmlString += "<img class=\"deleteMsgImg\" ";
                htmlString += "src=\"images/delete.png\" ";
                htmlString += "width=20 height=20 messageId=\"";
                htmlString += element.getAttribute('id');
                htmlString += "\" />";
                htmlString += "</td></tr>";
            }
        }
        htmlString += "</table>";
        this.smsDiv.html(htmlString);

        var self = this;

        $(".deleteMsgImg").click(function(){
              //alert($(this).attr("messageId"));
              $.ajax({
                  caller: self,
                  type: 'post',
                  url: 'linknx.php?action=cmd',
                  data: '<remove><message id=\"' + $(this).attr("messageId") + '\" /></remove>',
                  processData: false,
                  dataType: 'xml',
                  success: function(data) {
                      this.caller.retrieveMessages();
                  },
                  //complete: self.poll()
              })
        });

        $("#sendSms").click(function(){
              this.owner.showSendDialog();
        });
        $("#sendSms").get(0).owner = this;

        if (objs.length > 0 &&
            oldHtml != this.smsDiv.html())
        {
            var audioNode = this.div.find("audio");
            if (audioNode)
                audioNode[0].play(); 
            //$(".audioDemo").trigger('play');
            //this.div.find("audio").play();
        }
    }
}

cWidgetSMS.prototype.retrieveMessages = function(completecb)
{
  var self = this;

  $.ajax({
      caller: self,
      type: 'post',
      url: 'linknx.php?action=cmd',
      data: '<read><messages /></read>',
      processData: false,
      dataType: 'xml',
      success: function(data) {
          this.caller.refreshHtml(data.documentElement);
      },
      complete: completecb
  });
}

cWidgetSMS.prototype.poll = function()
{
  var self = this;

  setTimeout(function() {
      $.ajax({
          caller: self,
          type: 'post',
          url: 'linknx.php?action=cmd',
          data: '<read><messages /></read>',
          processData: false,
          dataType: 'xml',
          success: function(data) {
              this.caller.refreshHtml(data.documentElement);
          },
          complete: self.poll()
      })
  }, 60000);
}

cWidgetSMS.prototype.onUpdate = function(feedbackObj, value)
{
  //
}

cWidgetSMS.prototype.showSendDialog = function()
{
    var html = '<div id="dialog-message" title="Send SMS">';
    html    += '<form><fieldset>';
    html    += '<label for="to">To</label><input type="text" name="to" id="to" value="">';
    html    += '<label for="message">Message</label><input type="text" name="message" id="message" value="">';
    html    += '<!-- Allow form submission with keyboard without duplicating the dialog button -->';
    //html    += '<input type="submit" tabindex="-1" style="position:absolute; top:-1000px">';
    html    += '</fieldset>';
    html    += '</form>';

    var a=$(html);

    a.dialog({
        width: 300,
        //autosize: true,
        //resizable: false,
        autoopen: true,
        modal: true,
        buttons: {
            "Send": function() {
                sendSMS($("#to").val(), $("#message").val());
                $( this ).dialog( "close" );
            },
            Cancel: function() {
                a.dialog( "close" )
            },
        },
        close: function(event, ui) {
            $(this).dialog("destroy");
            $(this).remove();
        }
    });

    /*form = a.find( "form" ).on( "submit", function( event ) {
        event.preventDefault();
        sendSMS(($("#to").val(), ($("#message").val());
    });*/
}

function sendSMS(to, message)
{
    $.ajax({
        type: 'post',
        url: 'linknx.php?action=cmd',
        data: '<send><sms to=\"' + to + '\" text=\"' + message + '\" /></send>',
        processData: false,
        dataType: 'xml',
        success: function(data) {
            //
        },
    });
}

/******************************************************************************/

