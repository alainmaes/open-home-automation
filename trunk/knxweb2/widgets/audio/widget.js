function CAudio(conf) {
  this.isResizable=false;
  this.init(conf);
  
  this.value=0;
  this.min = 0;
  this.max = 100;
 
  $('#state', this.div )[0].owner = this;
  $("#state", this.div).click(function() {
    if (!this.owner.editMode)
    {
      this.owner.sendVolumeToAmpli(0);
    }
  });

  this.refreshHTML();
}

CAudio.type='audio';
UIController.registerWidget(CAudio);
CAudio.prototype = new CWidget();

// Refresh HTML from config
CAudio.prototype.refreshHTML = function()
{
  var conf = this.conf;  

  $(this.div).attr( "title", this.conf.getAttribute("feedback-object"));

  if (this.conf.getAttribute("border") == 'true') 
    this.div.css('border', "1px solid " + this.conf.getAttribute("border-color")); 
  else
    this.div.css('border','');

  /* set up the widget title */
  if (this.conf.getAttribute("titleLabel") != "")
    $('#title', this.div).text(this.conf.getAttribute("titleLabel"));
  else
    $('#title', this.div).text(this.conf.getAttribute("feedback-object"));
  $("#title", this.div).css('width', (this.div.width() - 32) + 'px');

  $("#state", this.div).css('background-image', 'url(' + getImageUrl("32x32_sonOff.png") + ')');
  $("#state", this.div).css('width', '32px');
  $("#state", this.div).css('height', '32px');
  $("#state", this.div).css('right', '32px');
  $("#state", this.div).css('top', '0px');
  //  slider.css('width', (this.div.width() - 30) + 'px');
  //  slider.css('left', '15px');

  /* set up the input selection box */
  if (this.conf.getAttribute("tunerLabel") != "")
    $('#inputSelection', this.div).append(
      '<option value="TUNE">' + this.conf.getAttribute("tunerLabel") + '</option>');
  else
    $('#inputSelection', this.div).append(
      '<option value="TUNE">Tuner</option>');
  if (this.conf.getAttribute("aux1Label") != "")
    $('#inputSelection', this.div).append(
      '<option value="AUX1">' + this.conf.getAttribute("aux1Label") + '</option>');
  else
    $('#inputSelection', this.div).append(
      '<option value="AUX1">Aux1</option>');
  if (this.conf.getAttribute("aux2Label") != "")
    $('#inputSelection', this.div).append(
      '<option value="AUX2">' + this.conf.getAttribute("aux2Label") + '</option>');
  else
    $('#inputSelection', this.div).append(
      '<option value="AUX2">Aux2</option>');
  if (this.conf.getAttribute("aux3Label") != "")
    $('#inputSelection', this.div).append(
      '<option value="AUX3">' + this.conf.getAttribute("aux3Label") + '</option>');
  else
    $('#inputSelection', this.div).append(
      '<option value="AUX3">Aux3</option>');
  if (this.conf.getAttribute("aux4Label") != "")
    $('#inputSelection', this.div).append(
      '<option value="AUX4">' + this.conf.getAttribute("aux4Label") + '</option>');
  else
    $('#inputSelection', this.div).append(
      '<option value="AUX4">Aux4</option>');
  
  var inputSelection = $('#inputSelection', this.div);
  inputSelection[0].owner = this;
  inputSelection.bind("change", function()
  {
    this.owner.sendInputToAmpli($("#inputSelection", this.owner.div).val());
  });

  /* Populate the channels array */
  var value;
  //var body = '<read><object id="' + arr[i] + '"/></read>';
  var body = '<read><config><objects/></config></read>';
  var req = jQuery.ajax(
  {
    type: 'post',
    url: 'linknx.php?action=cmd',
    data: body,
    processData: false,
    dataType: 'xml',
    async: false,
    success: function(responseXML, status) 
    {
        var xmlResponse = responseXML.documentElement;
        if (xmlResponse.getAttribute('status') != 'error')
        {
           objs = xmlResponse.getElementsByTagName('object');
	}
    }
  });

  var arr=this.conf.getAttribute("test").split(", ");
  var length = arr.length, element = null;
  for (var i = 0; i < length; i++)
  {
    if (arr[i] == "")
      continue;
  
    if (objs.length > 0)
    {
      for (i=0; i < objs.length; i++)
      {
        var element = objs[i];
        //if (element.getAttribute("id") == arr[i])
        //  $('#channelSelection', this.div).append(
        //    '<option value="' + element.getAttribute("address") + '">' + arr[i] + '</option>');
      }
    }
  }

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

  /*var freqSlider = $('#freqSlider', this.div);
  freqSlider.owner = this;

  freqSlider.slider( "destroy" );
  freqSlider.slider({  
        orientation: conf.getAttribute("orientation"),
        range: range,
        min: 875,
        max: 1080,
        //value: parseInt( delta / 2), 
        step: 1,
        //disabled: disabled,
    });

    freqSlider.css('width', '100%');
    freqSlider.css('left', '0px');
    //freqSlider.css('top', '20px');

    if (this.editMode)  {
        //this.updateObject(this.conf.getAttribute("feedback-object"),(( this.delta / 2 ) + this.min ));
    } else {
        $( '#freqSlider', this.div )[0].owner = this;
        freqSlider.bind( "slidestop", function(event, ui)
	{
            $('#frequency', this.owner.div).text(ui.value/10.0);
            this.owner.sendFrequencyToAmpli(ui.value);
            //$(".value",this.owner.div).hide();
        });
        freqSlider.bind( "slide", function(event, ui) {
            $('#frequency', this.owner.div).text(ui.value/10.0);
            this.owner.sendFrequencyToAmpli(ui.value);
            //$(".value",this.owner.div).text(value).show();
        });
    }*/

  /* volume slider */ 
  if (this.conf.getAttribute("min") != "") this.min = parseInt(this.conf.getAttribute("min"));
  if (this.conf.getAttribute("max") != "") this.max = parseInt(this.conf.getAttribute("max"));
   if (this.min < 0)
      this.min = 0;
  if (this.max > 100)
      this.max = 100;

  this.position = this.conf.getAttribute("position");
  this.orientation = this.conf.getAttribute("orientation");
  var disabled = false; //TODO

  if (this.min > this.max) {
    var max = this.max;
    this.max = this.min;
    this.min = max;
  }
 
  this.oldvalue = (this.min - 1);
 
 
  var delta = (this.max - this.min); 
  this.delta = delta;

  var step = parseInt( delta / (parseInt(this.conf.getAttribute("width"))) );
  if (step < 1) step = 1;

  var range = 'min';
  
  var slider = $('#slider', this.div);
  slider.owner = this;

  slider.slider( "destroy" );
  slider.slider({  
        orientation: conf.getAttribute("orientation"),
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

    
    //slider.css('width', '90%');
    //slider.css('left', '0px');
    //slider.css('top', '20px');
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

CAudio.prototype.sendVolumeToAmpli = function(volume) {
    if (this.editMode)
        return;

    if (volume < 16)
        newValue = "0" + volume.toString(16) + this.oldvalue.substr(2);
    else
        newValue = volume.toString(16) + this.oldvalue.substr(2);
 
    EIBCommunicator.eibWrite(
        this.conf.getAttribute("feedback-object"), newValue);
}

CAudio.prototype.sendInputToAmpli = function(input) {
    if (this.editMode)
        return;

    newValue = this.oldvalue.substr(0, 3) + input + this.oldvalue.substr(7);
 
    EIBCommunicator.eibWrite(
        this.conf.getAttribute("feedback-object"), newValue);
}

CAudio.prototype.sendFrequencyToAmpli = function(frequency) {
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

    EIBCommunicator.eibWrite(
        this.conf.getAttribute("feedback-object"), command);
}

CAudio.prototype.convertFromUiValue = function(valueui) {
  var value = valueui + this.min;
  return value;  
}

CAudio.prototype.convertToUiValue = function(value) {
  if (value < this.min ) value = this.min;
  else if (value > this.max ) value = this.max;
  var valueui = parseInt(value, 16) + parseInt(this.min);
  return valueui;  
}

// Called by eibcommunicator when a feedback object value has changed
CAudio.prototype.updateObject = function(obj,value) {
  if (obj==this.conf.getAttribute("feedback-object"))
  {
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

      /* update frequency */
      //$('#frequency', this.div).text(this.freq);
      /* var freqSlider = $('#freqSlider', this.div);
      freqSlider.slider("value", this.freq * 10); */

      /* update channel select box */
      var channelSelect = $('#channelSelection', this.div);
      //alert(this.freq);
      channelSelect.val(this.freq);
    }
  }
};
