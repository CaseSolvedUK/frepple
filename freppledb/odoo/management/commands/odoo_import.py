#
# Copyright (C) 2016 by frePPLe bv
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Affero General Public License as published
# by the Free Software Foundation; either version 3 of the License, or
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
# General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public
# License along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

from django.core import management
from django.core.management.base import BaseCommand, CommandError
from django.conf import settings
from django.db import DEFAULT_DB_ALIAS
from django.utils.translation import gettext_lazy as _
from django.template import Template, RequestContext

from freppledb import __version__


class Command(BaseCommand):

    help = "Loads data from an Odoo instance into the frePPLe database"

    requires_system_checks = []

    def get_version(self):
        return __version__

    def add_arguments(self, parser):
        parser.add_argument("--user", help="User running the command")
        parser.add_argument(
            "--database",
            default=DEFAULT_DB_ALIAS,
            help="Nominates the frePPLe database to load",
        )
        parser.add_argument(
            "--task",
            type=int,
            help="Task identifier (generated automatically if not provided)",
        )
        parser.add_argument(
            "--environment", default="odoo_read_1", help="data source environment"
        )

    def handle(self, **options):
        self.verbosity = int(options["verbosity"])
        task = options.get("task", None)
        database = options["database"]
        environment = options["environment"]
        if (
            not environment.startswith("odoo_read_")
            or not len(environment) == 11
            or not environment[10].isdigit()
        ):
            raise CommandError(
                "Invalid environment: %s. It must be odoo_read_X with X being a number between 1 to 5"
                % environment
            )
        if database not in settings.DATABASES.keys():
            raise CommandError("No database settings known for '%s'" % self.database)
        if options["user"]:
            management.call_command(
                "runplan",
                env=environment,
                database=database,
                user=options["user"],
                task=task,
            )
        else:
            management.call_command(
                "runplan", env=environment, database=database, task=task
            )

    # accordion template
    title = _("Import data from %(erp)s") % {"erp": "odoo"}
    index = 1400
    help_url = "integration-guide/odoo-connector.html"

    @staticmethod
    def getHTML(request):
        return Template(
            """
            {% load i18n %}
            <form role="form" method="post" action="{{request.prefix}}/execute/launch/odoo_import/">{% csrf_token %}
            <table>
              <tr>
                <td style="vertical-align:top; padding: 15px">
                   <button  class="btn btn-primary"  type="submit" value="{% trans "launch"|capfirst %}">{% trans "launch"|capfirst %}</button>
                </td>
                <td  style="padding: 0px 15px;">{% trans "Import Odoo data into frePPLe." %}
                </td>
              </tr>
            </table>
            </form>
          """
        ).render(RequestContext(request))
