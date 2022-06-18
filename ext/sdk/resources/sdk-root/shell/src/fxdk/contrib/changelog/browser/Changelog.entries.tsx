import React from 'react';

export interface ChangelogEntry {
  id: string,
  title: React.ReactNode,
  content: React.ReactNode,
}

export const changelogEntries: ChangelogEntry[] = [
  {
    id: '2021-10-28',
    title: 'October 28, 2021',
    content: (
      <>
        <h3>Farewell Theia, long live FXCode!</h3>
        <h5>FXCode - is our fork of Code - OSS, which is better known as  Microsoft's VS Code</h5>

        <ul>
          <li>
            With this update we can finally offer the best extensions compatibility, since it's an actual thing and not look-alike like Theia was
          </li>

          <li>
            Our additions like Resource Monitor and Client/Server Console have been reworked to fit their new home
          </li>

          <li>
            We plan to keep FXCode updated to the latest stable VS Code release, but expect delays for about 2-3 weeks, but most likely will happen faster
          </li>
        </ul>
      </>
    ),
  },
  {
    id: '2021-09-01',
    title: 'September 01, 2021',
    content: (
      <>
        <h3>
          How did all these project explorer bugs went unseen
          <img src="https://openmoji.org/data/color/svg/1F972.svg" width={32} height={32} style={{ verticalAlign: 'middle' }} />
        </h3>

        <ul>
          <li>
            You <em>should</em> be able to drag'n'drop anything into anything now
          </li>
          <li>
            That "next big thing", which is pretty much an open secret, is like 99.9% ready!
          </li>
        </ul>
      </>
    ),
  },
  {
    id: '2021-07-20',
    title: 'July 20, 2021',
    content: (
      <>
        <h3>We're almost ready for the next big thing in FxDK, stay tuned</h3>

        <ul>
          <li>
            Better indication of resource (and something else) status
          </li>
        </ul>
      </>
    ),
  },
  {
    id: '2021-05-14',
    title: 'May 14, 2021',
    content: (
      <>
        <ul>
          <li>
            ⚠️ If you were using <kbd>.fxdkignore</kbd> files in your project they were treated wrong as it was <kbd>.fxdkinclude</kbd>, this has been fixed.
            <br/>
            These files actually must contain list of rules for files to actually ignore during build.
          </li>
          <li>
            ⚠️ In-project <kbd>system-resources</kbd> has been <strong>deprecated</strong>, we'll do our best to upgrade your project when it opens.
            <br/>
            Though, if you did modify that folder we will not remove it, but only rename, <em>shame on you</em>!
          </li>
          <li>
            System resources needed for project can now be tweaked in <strong>Project Settings</strong>
          </li>
          <li>
            You can now import some example resources from cfx-server-data
          </li>
        </ul>
      </>
    ),
  },
  {
    id: '2021-05-08',
    title: 'May 8, 2021',
    content: (
      <>
        <ul>
          <li>
            Removed client resource monitor theia widget 😭
          </li>
          <li>
            Added combined (both client and server) resource monitor theia widget 🎉
            <br/>
            Do note that server data will only be available if using latest server artifact ⚠️
          </li>
        </ul>
      </>
    ),
  },
  {
    id: '2021-05-04',
    title: 'May 4, 2021',
    content: (
      <>
        <ul>
          <li>
            Added client resource monitor theia widget 🎉
            <br/>
            This is pretty basic for now, but it's a solid base for a tighter integration
          </li>
        </ul>
      </>
    ),
  },
  {
    id: '2021-03-24',
    title: 'March 24, 2021',
    content: (
      <>
        <ul>
          <li>
            Fixed game connecting to server too early, hopefully
          </li>
          <li>
            Fixed project explorer not showing correct fs tree when importing stuff add over 9000 of files in a blink
          </li>
        </ul>
      </>
    ),
  },
  {
    id: '2021-03-23',
    title: 'March 23, 2021',
    content: (
      <>
        <ul>
          <li>
            Fix most cases of EPERM while deleting/renaming folders, but not all of them:
            <ul>
              <li>
                tsserver of typescript-language-features ext of theia is still using node's broken file watcher
                so it will still EPERM if deleting/renaming folder containing tsconfig.json, rip
              </li>
              <li>
                omnisharp of csharp-language-features ext of theia will also block deleting/renaming of folder with cs resource, rip
              </li>
            </ul>
          </li>
        </ul>
      </>
    ),
  },
  {
    id: '2021-03-17',
    title: 'March 17, 2021',
    content: (
      <>
        <ul>
          <li>
            Fixed theia's terminal not working
          </li>
          <li>
            Added directory rename (how did I forget about that 🧐)
          </li>
          <li>
            Hopefully fixed rare deadlocks during opening/creating project
          </li>
        </ul>
      </>
    ),
  },
  {
    id: '2021-03-16',
    title: 'March 16, 2021',
    content: (
      <>
        <ul>
          <li>
            Fixed some system resources not actually be enabled right after project creation
          </li>
          <li>
            Added default project build path: for project <kbd>C</kbd> in <kbd>/a/b/c</kbd> default build path will be <kbd>/a/b/c-build</kbd>
          </li>
          <li>
            Added ability to resize FxDK's toolbar
          </li>
        </ul>
      </>
    ),
  },
  {
    id: '2021-03-13',
    title: 'March 13, 2021',
    content: (
      <>
        <ul>
          <li>
            Issue with different game builds not connecting to server has been fixed
          </li>
        </ul>
      </>
    ),
  },
  {
    id: '2021-03-12',
    title: 'March 12, 2021',
    content: (
      <>
        <ul>
          <li>
            <strong>Ditched embedded filesystem viewer (as seen in project creator) in favor of system's folder select dialog</strong>
            <br/>
            This is due to a big amount of unforeseen edge-cases that sometimes cause viewer to be completely broken.
          </li>
        </ul>
      </>
    ),
  },
  {
    id: 'initial',
    title: 'FxDK Pre-release 🎉',
    content: (
      <>
        <p>
          <strong>23 february 2021</strong>
        </p>
        <ul>
          <li>FxDK now supports recycling deleted items, woo</li>
          <li>Resource rename leading to shell crash got fixed, oops</li>
        </ul>

        <p>
          <strong>17 february 2021</strong>
        </p>
        <ul>
          <li>Project builder just got better, it is now capable of building "complete" server</li>
        </ul>

        <p>
          <strong>12 february 2021</strong>
        </p>
        <ul>
          <li>Right-click to copy selection while in client/server console</li>
        </ul>

        <p>
          <strong>10 february 2021</strong>
        </p>
        <ul>
          <li>You can now drag'n'drop files or folders from desktop to folders/resources in fxdk</li>
          <li>
            Server is running with <kbd>sv_fxdkMode=1</kbd> convar now,
            so you can use different logic for fetching players tokens/identifiers while in fxdk environment.
            <br/>
            Test it like so: <kbd>const isFxdk = GetConvarInt('sv_fxdkMode', 0) == 1</kbd>.
          </li>
        </ul>

        <p>
          <strong>8 february 2021</strong>
        </p>
        <ul>
          <li>Basic project builder</li>
          <li>Theia updated from 1.9.0 to 1.10.0</li>
          <li>Game menu of theia is now "FxDK" with two more options</li>
        </ul>

        <p>
          <strong>3 february 2021</strong>
        </p>
        <ul>
          <li>"Open in Explorer" context menu item available in ProjectExplorer now</li>
        </ul>

        <p>
          <strong>1 february 2021</strong>
        </p>
        <ul>
          <li>Welcome client console as well<br/>(Game -&gt; Toggle Client Console)</li>
        </ul>

        <p>
          <strong>29 january 2021</strong>
        </p>
        <ul>
          <li>Server console moved from server configuration modal to dedicated theia widget<br/>(Game -&gt; Toggle Server Console)</li>
          <li>Server console is now able to accept commands from you right in FxDK, woo</li>
        </ul>

        <p>
          <strong>20 january 2021</strong>
        </p>
        <ul>
          <li>If game crashes it will be restarted automatically, if it was connected to built-in server it will reconnect upon restart as well.</li>
        </ul>

        <p>
          <strong>18 january 2021</strong>
        </p>
        <ul>
          <li>Resource watch commands are now more visible</li>
          <li>You can now see watch commands output (Right click on resource -&gt; Watch commands output)</li>
        </ul>

        <p>
          <strong>14 january 2021</strong>
        </p>
        <ul>
          <li>Resource templates in resource creator</li>
          <li>
            C# template enabled autobuild-on-change for .cs files in projects
            <br />
            (you need .NET installed to create resource with C# template)
          </li>
          <li>
            Specify <kbd>fxdk_watch_command</kbd> in your <kbd>fxmanifest.lua</kbd>,
            <br />
            FxDK will run it if resource is enabled AND restart-on-change is on
          </li>
        </ul>

        <p>
          <strong>30 december 2020</strong>
        </p>
        <ul>
          <li>Full CSharp language support (w/o proper resource creation, yet)</li>
          <li>Move theia plugins out of theia distribution package, as omnisharp extension downloads <em>a lot</em> of files afterwards</li>
        </ul>

        <p>
          <strong>22 december 2020</strong>
        </p>
        <ul>
          <li>FxDK-flavored ayu theme by default</li>
          <li>Check if 30120 port is available for built-in fxserver upon start</li>
        </ul>

        <p>
          <strong>21 december 2020</strong>
        </p>
        <ul>
          <li>Show toolbar immediately after project open/create process begins</li>
          <li>Copy-paste functionality for folders and files</li>
        </ul>

        <p>
          <strong>18 december 2020</strong>
        </p>
        <ul>
          <li>Compact FxDK toolbar design, allowing more space for project explorer</li>
          <li>Granular-ish resources state propagation to shell-client</li>
          <li>Some tasks are now reporting their state including server installer bits</li>
        </ul>

        <p>
          <strong>16 december 2020</strong>
        </p>
        <ul>
          <li>Welcome changelog</li>
          <li>Improved preparing screen</li>
          <li>Lots of internal work as well</li>
        </ul>
      </>
    ),
  },
];
