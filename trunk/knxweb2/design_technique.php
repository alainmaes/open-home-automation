<?php

require_once "include/common.php";

error_reporting(0);

header('Content-Type: application/xml; charset=utf-8');

if (isset($_GET['action'])) {
	switch ($_GET['action']) 
	{
		case 'savedesign':
			$name = "default";
	    if (isset($_GET['name']))
        $name = $_GET['name'];
			$version = "design";
	    if (isset($_GET['ver']))
        $version = $_GET['ver'];
			if (ereg("[\\/.$;!?]", $name.$version) > 0)
				print("<savedesign status='error'>Restricted character in design or version name");
			elseif ($fp = fopen("design/".$name."/".$version.".xml", 'w')) {
			    $conf = file_get_contents("php://input");
				fwrite($fp, $conf);
				fclose($fp);
				print("<savedesign status='success'>");
			}
			elseif (!is_writable("design/".$name."/".$version.".xml"))
				print("<savedesign status='error'>Design has no write permission on server");
			else
				print("<savedesign status='error'>Unable to write design to file");
			print("</savedesign>\n");
	    break;
	    
		case 'designlist':
			if ($dh = opendir("design")) {
				print("<designlist status='success'>\n");
				while (($file = readdir($dh)) !== false) {
					if ($file != "." && $file != ".." && $file != "CVS" && is_dir("design/".$file)) {
						echo "<design name='$file'>\n";
						if ($fh = opendir("design/".$file)) {
							while (($ver = readdir($fh)) !== false) {
								if (substr($ver, -4) == ".xml")
									echo "<version>".substr($ver, 0, -4)."</version>\n";
							}
							closedir($fh);
						}
						echo "</design>\n";
					}
				}
				closedir($dh);
			}
			else
				print("<designlist status='error'>Unable to find design folder on server\n");
			print("</designlist>\n");
	    break;
	    
		case 'createdesign':
	    if (isset($_GET['name'])) {
        $name = $_GET['name'];
				if (file_exists("design/".$name))
					print("<createdesign status='error'>Design already exists");
				elseif (ereg("[\\/.$;!?]", $name) > 0)
					print("<createdesign status='error'>Restricted character in design name");
				elseif (mkdir("design/".$name, 0777) == false)
					print("<createdesign status='error'>Unable to create design folder");
				elseif ($fp = fopen("design/".$name."/design.xml", 'w')) {
					fwrite($fp, "<?xml version='1.0'?><config width='1280' height='1024' enableSlider='false'><zones/></config>\n");
					fclose($fp);
					print("<createdesign status='success'>");
				}
				else
					print("<createdesign status='error'>Unable to save new design");
			}
			else
				print("<createdesign status='error'>No design name specified");
			print("</createdesign>\n");
	    break;
      
	  case 'removedesign':
	    if (isset($_GET['name'])) {
        $name = $_GET['name'];
				if (!file_exists("design/".$name))
					print("<removedesign status='error'>Design not exists");
				else {
          $delfile = unlink("design/".$name."/design.xml");
          $deldir = rmdir("design/".$name);
          if ($delfile == "false")
					  print("<removedesign status='error'>Unable to remove design file");
          elseif ( $deldir == "false" )
					  print("<removedesign status='error'>Unable to remove design folder");
          else
					  print("<removedesign status='success'>");
        }
			}
			else
				print("<removedesign status='error'>No design name specified");
			print("</removedesign>\n");
	    break;
      
		case 'savefile':
			print("<savefile status='error'>File save is not possible, please put the file manually in design folder on server</savefile>\n");
	    break;
	    
		case 'filelist':
			$name = "default";
	    if (isset($_GET['name']))
        $name = $_GET['name'];
			if ($dh = opendir("design/".$name)) {
				print("<filelist status='success'>\n");
				while (($file = readdir($dh)) !== false) {
					if ($file != "." && $file != ".." && substr($file, -4) != ".xml")
						echo "<file>$file</file>\n";
				}
				closedir($dh);
			}
			else
				print("<filelist status='error'>Unable to find design '$name' on server\n");
			print("</filelist>\n");
	    break;
	    
		case 'filelistdir':
			$name = "images";
			if (isset($_GET['name']))	$name = $_GET['name'];
			
			$files=glob($_config['imageDir'] . $name . "*");

			echo("<filelist status='success'>\n");
			foreach($files as $f) {
				if (is_dir($f)) echo "<directory>" . basename($f) . "</directory>\n";
				if (is_file($f)) echo "<file>" . basename($f) . "</file>\n";
			}

				
/*				if ($dh = opendir($name)) {
					print("<filelist status='success'>\n");
					while (($file = readdir($dh)) !== false) {
						if ($file != "." && $file != ".." && substr($file, -4) != ".xml")
							echo "<file>$file</file>\n";
						}
					closedir($dh); */
			print("</filelist>\n");
			break;
			
		case 'saveconfig':
			$dir = "include";
	    if (isset($_GET['dir']))
        $dir = $_GET['dir'];
			if (ereg("[\\/.$;!?]", $dir) > 0)
				print("<saveconfig status='error'>Restricted character in dir");
			elseif ($fp = fopen($dir."/config.xml", 'w')) {
			    $conf = file_get_contents("php://input");
				fwrite($fp, $conf);
				fclose($fp);
				print("<saveconfig status='success'>");
			}
			elseif (!is_writable($dir."/config.xml"))
				print("<saveconfig status='error'>Config.xml has no write permission on server");
			else
				print("<saveconfig status='error'>Unable to create config.xml file");
			print("</saveconfig>\n");
	    break;
	    
		case 'newWidget':
			$w = getWidget($_GET['type']);
			
			$xml = new SimpleXMLElement("<control></control>");
			$xml->addAttribute("type", $w['name']);
			
			foreach($w['settings'] as $s) {
				if (isset($s['id'])) $xml->addAttribute($s['id'], ((isset($s['default']))?$s['default']:'') );
			}
			
			echo $xml->asXML();
			break;

		case 'savesubpages':
			if (file_put_contents("design/subpages.xml", file_get_contents("php://input")))
				echo "<savesubpages status='success'>";
			elseif (!is_writable("design/".$name."/".$version.".xml"))
				echo "<savesubpages status='error'>design/subpages.xml directory has no write permission on server";
			else
				echo "<savesubpages status='error'>Unable to write design to file";
			echo "</savesubpages>\n";
	    break;

		case 'updatewidgetscss':
			if ($fp = fopen("widgets/widgets.css", 'w')) {
				$conf = file_get_contents("php://input");
				fwrite($fp, $conf);
				fclose($fp);
				print("<updatewidgetscss status='success'>");
			}
			elseif (!is_writable("widgets/widgets.css"))
				print("<updatewidgetscss status='error'>widgets/widgets.css has no write permission on server");
			else
				print("<updatewidgetscss status='error'>Unable to create widgets/widgets.css file");
			print("</updatewidgetscss>\n");
	    break;

		default:
			print("<response status='error'>Unknown action</response>\n");
	    break;
	}
}
else
	print("<response status='error'>No action specified</response>\n");
?>